/******************************************************************************/
/* Important Fall 2014 CSCI 402 usage information:                            */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/******************************************************************************/

#include "kernel.h"
#include "config.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"
#include "proc/proc.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/kmalloc.h" 

#include "vm/vmmap.h"

#include "fs/vfs.h"
#include "fs/vfs_syscall.h"
#include "fs/vnode.h"
#include "fs/file.h"

proc_t *curproc = NULL; /* global */
static slab_allocator_t *proc_allocator = NULL;

static list_t _proc_list;
static proc_t *proc_initproc = NULL; /* Pointer to the init process (PID 1) */

void
proc_init()
{
        list_init(&_proc_list);
        proc_allocator = slab_allocator_create("proc", sizeof(proc_t));
        KASSERT(proc_allocator != NULL);
}

proc_t *
proc_lookup(int pid)
{
        proc_t *p;
        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                if (p->p_pid == pid) {
                        return p;
                }
        } list_iterate_end();
        return NULL;
}

list_t *
proc_list()
{
        return &_proc_list;
}

static pid_t next_pid = 0;

/**
 * Returns the next available PID.
 *
 * Note: Where n is the number of running processes, this algorithm is
 * worst case O(n^2). As long as PIDs never wrap around it is O(n).
 *
 * @return the next available PID
 */
static int
_proc_getid()
{
        proc_t *p;
        pid_t pid = next_pid;
        while (1) {
failed:
                list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                        if (p->p_pid == pid) {
                                if ((pid = (pid + 1) % PROC_MAX_COUNT) == next_pid) {
                                        return -1;
                                } else {
                                        goto failed;
                                }
                        }
                } list_iterate_end();
                next_pid = (pid + 1) % PROC_MAX_COUNT;
                return pid;
        }
}

/*
 * The new process, although it isn't really running since it has no
 * threads, should be in the PROC_RUNNING state.
 *
 * Don't forget to set proc_initproc when you create the init
 * process. You will need to be able to reference the init process
 * when reparenting processes to the init process.
 */
proc_t *
proc_create(char *name)
{
	proc_t * proc = (proc_t *) slab_obj_alloc(proc_allocator);

	memset(proc, 0, sizeof(proc_t));
	/*Use get_next pid*/
        proc->p_pid = _proc_getid();                 /* our pid */
        
        KASSERT(PID_IDLE!=proc->p_pid || list_empty(&_proc_list) );
        dbg(DBG_PRINT, "(GRADING1A 2.a): pid can only be PID_IDLE when this is the first process\n");
        KASSERT(PID_IDLE!=proc->p_pid || PID_IDLE==proc->p_pid );
        dbg(DBG_PRINT, "(GRADING1A 2.a): pid is only PID_INIT when creating from idle process\n");

	KASSERT(strlen(name) <= PROC_NAME_LEN);
        memcpy(proc->p_comm, name, strlen(name)); /* process name */

        list_init(&(proc->p_threads));
        list_init(&(proc->p_children));

        proc->p_status = 0;        /* exit status */
        proc->p_state = PROC_RUNNING;         /* running/sleeping/etc. */
	list_init(&(proc->p_wait.tq_list));          /* queue for wait(2) */
	proc->p_wait.tq_size = 0;          /* queue for wait(2) */

        proc->p_pagedir = pt_get();/*pagedir_t      *p_pagedir;*/

	list_link_init(&proc->p_list_link);
	list_link_init(&proc->p_child_link);
	
	list_insert_tail(&_proc_list, &proc->p_list_link);

	if(proc->p_pid > 0)
	{
		KASSERT(curproc);
		proc->p_pproc = curproc;         /* our parent process */
		list_insert_tail(&curproc->p_children, &proc->p_child_link);

#ifdef __VFS__
		unsigned int i = 0;
		for(i = 0; i < NFILES; i++)
		{
			proc->p_files[i] = curproc->p_files[i];

			if(proc->p_files[i])
				fref(proc->p_files[i]);
		}
#endif
	}

#ifdef __VFS__
	if(proc->p_pid > 2)
	{
		KASSERT(curproc->p_cwd);
		proc->p_cwd = curproc->p_cwd;
		vref(proc->p_cwd);
	}
#endif

	if(proc->p_pid == 1)
		proc_initproc = proc;

	return proc;
}

/**
 * Cleans up as much as the process as can be done from within the
 * process. This involves:
 *    - Closing all open files (VFS)
 *    - Cleaning up VM mappings (VM)
 *    - Waking up its parent if it is waiting
 *    - Reparenting any children to the init process
 *    - Setting its status and state appropriately
 *
 * The parent will finish destroying the process within do_waitpid (make
 * sure you understand why it cannot be done here). Until the parent
 * finishes destroying it, the process is informally called a 'zombie'
 * process.
 *
 * This is also where any children of the current process should be
 * reparented to the init process (unless, of course, the current
 * process is the init process. However, the init process should not
 * have any children at the time it exits).
 *
 * Note: You do _NOT_ have to special case the idle process. It should
 * never exit this way.
 *
 * @param status the status to exit the process with
 */
void
proc_cleanup(int status)
{
        KASSERT(NULL!=proc_initproc);
        dbg(DBG_PRINT, "(GRADING1A 2.b): INIT process does exist\n");
        KASSERT(1<=curproc->p_pid);
        dbg(DBG_PRINT, "(GRADING1A 2.b):This process is not the IDLE process\n");
        KASSERT(NULL!=curproc->p_pproc);
        dbg(DBG_PRINT, "(GRADING1A 2.b):This process has a parent process\n");
        
	proc_t * proc = NULL;
	curproc->p_status = status;
	curproc->p_state = PROC_DEAD;

	if(!sched_queue_empty(&curproc->p_pproc->p_wait))
	{
		sched_wakeup_on(&curproc->p_pproc->p_wait);
	}	
	
	if((curproc != proc_initproc) && !list_empty(&curproc->p_children))
	{
		list_iterate_begin(&curproc->p_children, proc, proc_t, p_child_link) {
			list_remove(&proc->p_child_link);
			
			list_insert_tail(&proc_initproc->p_children, &proc->p_child_link);

		} list_iterate_end();
	}
	
	int fd = 0;
	for (fd = 0; fd < NFILES; fd++) {
		if (curproc->p_files[fd])
			do_close(fd);
	}

	vput(curproc->p_cwd);
	
	KASSERT(NULL!=curproc->p_pproc);
        dbg(DBG_PRINT, "(GRADING1A 2.b):This process still has a parent process\n");
}

/*static kthread_t *
ktqueue_dequeue2(ktqueue_t *q)
{
        kthread_t *thr;
        list_link_t *link;

        if (list_empty(&q->tq_list))
                return NULL;

        link = q->tq_list.l_prev;
        thr = list_item(link, kthread_t, kt_qlink);
        list_remove(link);
        thr->kt_wchan = NULL;

        q->tq_size--;

        return thr;
}*/

/**
 * This has nothing to do with signals and kill(1).
 *
 * Calling this on the current process is equivalent to calling
 * do_exit().
 *
 * In Weenix, this is only called from proc_kill_all.
 */
void
proc_kill(proc_t *p, int status)
{
	KASSERT(p != curproc);
	kthread_t  * removeThisThread = NULL;
	p->p_status = -1;
	removeThisThread = list_head(&p->p_threads,kthread_t,kt_plink); 

	kthread_cancel(removeThisThread, NULL);  
}

/*
 * Remember, proc_kill on the current process will _NOT_ return.
 * Don't kill direct children of the idle process.
 *
 * In Weenix, this is only called by sys_halt.
 */
void
proc_kill_all()
{
	proc_t * processToKill;
	list_iterate_begin(&_proc_list,processToKill,proc_t,p_list_link)
	{
		if((processToKill->p_pid!=0) && 
			(processToKill->p_pid!=curproc->p_pid) && 
			(processToKill->p_pproc->p_pid != 0))
		{
			/* review status */
			proc_kill(processToKill, -1);   
		}

	}list_iterate_end();

     do_exit(0);

}

/*
 * This function is only called from kthread_exit.
 *
 * Unless you are implementing MTP, this just means that the process
 * needs to be cleaned up and a new thread needs to be scheduled to
 * run. If you are implementing MTP, a single thread exiting does not
 * necessarily mean that the process should be exited.
 */
void
proc_thread_exited(void *retval)
{
	/* Need to figure out actual status parameter */
	 if (curproc->p_status < 0)
	 {
		dbg(DBG_PRINT,"(GRADING2C 1) : p_status is < 0.\n");
		proc_cleanup(curproc->p_status);
	}			
	else
	{ 
	     dbg(DBG_PRINT,"(GRADING2B) : p_status is >= 0.\n");
	     proc_cleanup(0);
	 }    	

	/* Then need to unlock any mutexes locked by this thread */

        /*NOT_YET_IMPLEMENTED("PROCS: proc_thread_exited");*/
}

/* If pid is -1 dispose of one of the exited children of the current
 * process and return its exit status in the status argument, or if
 * all children of this process are still running, then this function
 * blocks on its own p_wait queue until one exits.
 *
 * If pid is greater than 0 and the given pid is a child of the
 * current process then wait for the given pid to exit and dispose
 * of it.
 *
 * If the current process has no children, or the given pid is not
 * a child of the current process return -ECHILD.
 *
 * Pids other than -1 and positive numbers are not supported.
 * Options other than 0 are not supported.
 */
pid_t
do_waitpid(pid_t pid, int options, int *status)
{
	KASSERT(curproc->p_wait.tq_size == 0);
	proc_t * proc = NULL;
	proc_t * dead_proc = NULL;
	kthread_t * dead_thread = NULL;
	
	pid_t pid_out = 0;

	if(list_empty(&curproc->p_children))
		return -ECHILD;
	
	if(pid == -1)
	{
		KASSERT(pid==-1);
		dbg(DBG_PRINT, "(GRADING1A 2.c): Able to find a valid process ID for the process\n");
		do
		{	
			list_iterate_begin(&curproc->p_children, proc, proc_t, p_child_link) {
				if(proc->p_state == PROC_DEAD)
					dead_proc = proc;

			} list_iterate_end();

			if(!dead_proc)
				sched_sleep_on(&curproc->p_wait);
			else
				break;

		}while(1);
	}
	else
	{
		list_iterate_begin(&curproc->p_children, proc, proc_t, p_child_link) {
			if(proc->p_pid == pid)
				dead_proc = proc;

		} list_iterate_end();
                
                
		if(dead_proc)
		{        
		        KASSERT(dead_proc->p_pid==pid);
                        dbg(DBG_PRINT, "(GRADING1A 2.c): Able to find a valid process ID for the process\n");
			while(dead_proc->p_state != PROC_DEAD)
			{	
				sched_sleep_on(&curproc->p_wait);
			}
		}
		else
			return -ECHILD;
	}
	
	KASSERT(NULL!=dead_proc);
	dbg(DBG_PRINT, "(GRADING1A 2.c): The dead process is not NULL\n");

	*status = dead_proc->p_status;
	
	pid_out = dead_proc->p_pid;
	list_remove(&dead_proc->p_list_link);
	list_remove(&dead_proc->p_child_link);

	dead_thread = list_head(&dead_proc->p_threads, kthread_t, kt_plink);
	KASSERT(KT_EXITED==dead_thread->kt_state);
	dbg(DBG_PRINT, "(GRADING1A 2.c): Thread points to a thread to be destroyed\n");
	KASSERT(NULL!=curproc->p_pagedir);
	dbg(DBG_PRINT, "(GRADING1A 2.c): This process has a vaild page directory\n");

	/* cleanup tcb and pcb*/
	kthread_destroy(dead_thread);
	slab_obj_free(proc_allocator, dead_proc); 

	return pid_out;
}

/*
 * Cancel all threads, join with them, and exit from the current
 * thread.
 *
 * @param status the exit status of the process
 */
void
do_exit(int status)
{
	curproc->p_status=status;
	kthread_exit(NULL);
        /*NOT_YET_IMPLEMENTED("PROCS: do_exit");*/
}

size_t
proc_info(const void *arg, char *buf, size_t osize)
{
        const proc_t *p = (proc_t *) arg;
        size_t size = osize;
        proc_t *child;

        KASSERT(NULL != p);
        KASSERT(NULL != buf);

        iprintf(&buf, &size, "pid:          %i\n", p->p_pid);
        iprintf(&buf, &size, "name:         %s\n", p->p_comm);
        if (NULL != p->p_pproc) {
                iprintf(&buf, &size, "parent:       %i (%s)\n",
                        p->p_pproc->p_pid, p->p_pproc->p_comm);
        } else {
                iprintf(&buf, &size, "parent:       -\n");
        }

#ifdef __MTP__
        int count = 0;
        kthread_t *kthr;
        list_iterate_begin(&p->p_threads, kthr, kthread_t, kt_plink) {
                ++count;
        } list_iterate_end();
        iprintf(&buf, &size, "thread count: %i\n", count);
#endif

        if (list_empty(&p->p_children)) {
                iprintf(&buf, &size, "children:     -\n");
        } else {
                iprintf(&buf, &size, "children:\n");
        }
        list_iterate_begin(&p->p_children, child, proc_t, p_child_link) {
                iprintf(&buf, &size, "     %i (%s)\n", child->p_pid, child->p_comm);
        } list_iterate_end();

        iprintf(&buf, &size, "status:       %i\n", p->p_status);
        iprintf(&buf, &size, "state:        %i\n", p->p_state);

#ifdef __VFS__
#ifdef __GETCWD__
        if (NULL != p->p_cwd) {
                char cwd[256];
                lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                iprintf(&buf, &size, "cwd:          %-s\n", cwd);
        } else {
                iprintf(&buf, &size, "cwd:          -\n");
        }
#endif /* __GETCWD__ */
#endif

#ifdef __VM__
        iprintf(&buf, &size, "start brk:    0x%p\n", p->p_start_brk);
        iprintf(&buf, &size, "brk:          0x%p\n", p->p_brk);
#endif

        return size;
}

size_t
proc_list_info(const void *arg, char *buf, size_t osize)
{
        size_t size = osize;
        proc_t *p;

        KASSERT(NULL == arg);
        KASSERT(NULL != buf);

#if defined(__VFS__) && defined(__GETCWD__)
        iprintf(&buf, &size, "%5s %-13s %-18s %-s\n", "PID", "NAME", "PARENT", "CWD");
#else
        iprintf(&buf, &size, "%5s %-13s %-s\n", "PID", "NAME", "PARENT");
#endif

        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                char parent[64];
                if (NULL != p->p_pproc) {
                        snprintf(parent, sizeof(parent),
                                 "%3i (%s)", p->p_pproc->p_pid, p->p_pproc->p_comm);
                } else {
                        snprintf(parent, sizeof(parent), "  -");
                }

#if defined(__VFS__) && defined(__GETCWD__)
                if (NULL != p->p_cwd) {
                        char cwd[256];
                        lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                        iprintf(&buf, &size, " %3i  %-13s %-18s %-s\n",
                                p->p_pid, p->p_comm, parent, cwd);
                } else {
                        iprintf(&buf, &size, " %3i  %-13s %-18s -\n",
                                p->p_pid, p->p_comm, parent);
                }
#else
                iprintf(&buf, &size, " %3i  %-13s %-s\n",
                        p->p_pid, p->p_comm, parent);
#endif
        } list_iterate_end();
        return size;
}
