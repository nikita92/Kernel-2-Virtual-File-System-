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

/*
 *  FILE: open.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Mon Apr  6 19:27:49 1998
 */

#include "globals.h"
#include "errno.h"
#include "fs/fcntl.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/stat.h"
#include "util/debug.h"

/* find empty index in p->p_files[] */
	int
get_empty_fd(proc_t *p)
{
	int fd;

	for (fd = 0; fd < NFILES; fd++) {
		if (!p->p_files[fd])
			return fd;
	}

	dbg(DBG_ERROR | DBG_VFS, "ERROR: get_empty_fd: out of file descriptors "
			"for pid %d\n", curproc->p_pid);
	return -EMFILE;
}

/*
 * There a number of steps to opening a file:
 *      1. Get the next empty file descriptor.
 *      2. Call fget to get a fresh file_t.
 *      3. Save the file_t in curproc's file descriptor table.
 *      4. Set file_t->f_mode to OR of FMODE_(READ|WRITE|APPEND) based on
 *         oflags, which can be O_RDONLY, O_WRONLY or O_RDWR, possibly OR'd with
 *         O_APPEND or O_CREAT.
 *      5. Use open_namev() to get the vnode for the file_t.
 *      6. Fill in the fields of the file_t.
 *      7. Return new fd.
 *
 * If anything goes wrong at any point (specifically if the call to open_namev
 * fails), be sure to remove the fd from curproc, fput the file_t and return an
 * error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        oflags is not valid.
 *      o EMFILE
 *        The process already has the maximum number of files open.
 *      o ENOMEM
 *        Insufficient kernel memory was available.
 *      o ENAMETOOLONG
 *        A component of filename was too long.
 *      o ENOENT
 *        O_CREAT is not set and the named file does not exist.  Or, a
 *        directory component in pathname does not exist.
 *      o EISDIR
 *        pathname refers to a directory and the access requested involved
 *        writing (that is, O_WRONLY or O_RDWR is set).
 *      o ENXIO
 *        pathname refers to a device special file and no corresponding device
 *        exists.
 */

int
do_open(const char *filename, int oflags)
{
	int next_fd = get_empty_fd(curproc);
	vnode_t *res_vnode =NULL;

	if(next_fd == -EMFILE){
		dbg(DBG_PRINT,"There are no more fd's available ");
		return -EMFILE;
	}

	file_t * file = fget(-1);	

	if(file == NULL){
		dbg(DBG_PRINT," Memory Unavailable ");
		fput(file);
		return -ENOMEM;
	}
	if(strlen(filename)>NAME_LEN){
		dbg(DBG_PRINT, " File name too long ");
		fput(file);
		return -ENAMETOOLONG;
	}

	if((oflags == (O_RDONLY|O_CREAT)) || (oflags == (O_RDONLY|O_TRUNC)) || oflags == (O_RDONLY)){
		file->f_mode = FMODE_READ;
	}else if( (oflags == (O_WRONLY|O_CREAT) || (oflags ==(O_WRONLY|O_TRUNC)) || (oflags ==O_WRONLY))){
		file->f_mode = FMODE_WRITE;
	}else if((oflags == (O_RDWR)) || (oflags == (O_RDWR|O_CREAT)) || (oflags == (O_RDWR|O_TRUNC))){
		file->f_mode = FMODE_WRITE|FMODE_READ;
	}else if(oflags == (O_RDONLY|O_APPEND)){
		file->f_mode = FMODE_READ|FMODE_APPEND;
	}else if(oflags == (O_WRONLY|O_APPEND)){
		file->f_mode = FMODE_WRITE|FMODE_APPEND;
	}else if(oflags == (O_RDWR|O_APPEND)){
		file->f_mode = FMODE_WRITE|FMODE_READ|FMODE_APPEND;
	}else{
		dbg(DBG_PRINT," Invalid device");
		fput(file);
		return -EINVAL;
	}



	/*int open_ret = open_namev(filename,file->f_mode,&res_vnode,file->f_vnode);*/
	int open_ret = open_namev(filename,oflags,&res_vnode,NULL);
	if(open_ret!=0)
	{
			fput(file);
			return open_ret;
	}

	if(S_ISDIR(res_vnode->vn_mode) && ((oflags & 0x03)==O_WRONLY || (oflags & 0x03)==O_RDWR)){
		dbg(DBG_PRINT," The res_vnode refers to a character or block device");
		fput(file);
		vput(res_vnode);
		return -EISDIR;
	}
	if((S_ISBLK(res_vnode->vn_mode)&&(blockdev_lookup(res_vnode->vn_devid)==NULL)) || ((bytedev_lookup(res_vnode->vn_devid)==NULL) && S_ISCHR(res_vnode->vn_mode))){
		dbg(DBG_PRINT," The res_vnode refers to a character or block device");
		fput(file);
		vput(res_vnode);
		return -ENXIO;
	}

	file->f_vnode = res_vnode;
	curproc->p_files[next_fd] = file;
	return next_fd;
        /*NOT_YET_IMPLEMENTED("VFS: do_open");
        return -1;*/
}
