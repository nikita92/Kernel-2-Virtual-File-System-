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
 *  FILE: vfs_syscall.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Wed Apr  8 02:46:19 1998
 *  $Id: vfs_syscall.c,v 1.7 2014/09/21 19:44:42 cvsps Exp $
 */

#include "kernel.h"
#include "errno.h"
#include "globals.h"
#include "fs/vfs.h"
#include "fs/file.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/fcntl.h"
#include "fs/lseek.h"
#include "mm/kmalloc.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/stat.h"
#include "util/debug.h"

/* To read a file:
 *      o fget(fd)
 *      o call its virtual read fs_op
 *      o update f_pos
 *      o fput() it
 *      o return the number of bytes read, or an error
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for reading.
 *      o EISDIR
 *        fd refers to a directory.
 *
 * In all cases, be sure you do not leak file refcounts by returning before
 * you fput() a file that you fget()'ed.
 */
int
do_read(int fd, void *buf, size_t nbytes)
{
	 if(fd>=NFILES||fd<0||curproc->p_files[fd]==NULL){
		dbg(DBG_PRINT, "(GRADING2B ): fd is not a valid file descriptor or is not open for reading\n");
		return -EBADF;
	}
	/* 1. getting the file descriptor.
	 * So given a file descriptor we get the
	 * correspong DS file_t which is similar
	 * to our extension context*/
	file_t *cur_fd = fget(fd);
	if(cur_fd->f_mode!=1 && cur_fd->f_mode!=3 && cur_fd->f_mode!=5 && cur_fd->f_mode!=7) 
        {
           dbg(DBG_PRINT, "(GRADING2B ): File not allowed to be read \n");
           fput(cur_fd); /* When fd is invalid, reqfile is null, fget does not do a fref(), so need to fput() */ 
           return -EBADF;
        } 
	/* 2. Call its virtual read fs_op
	 * vnode ,off_t offset, void *buf, size_t count
	 *
     * The current position in the file. Can be modified by system calls
     * like lseek(2), read(2), and write(2) (and possibly others) as
     * described in the man pages of those calls.
     */
        if(S_ISDIR(cur_fd->f_vnode->vn_mode))   
        {
         dbg(DBG_PRINT, "(GRADING2B ): File is a directory \n");
          fput(cur_fd);  
          return -EISDIR;
        }

	int read_ret = (cur_fd->f_vnode->vn_ops->read)(cur_fd->f_vnode,cur_fd->f_pos,buf,nbytes);
	/*if(read_ret<0){
		dbg(DBG_PRINT, "Error occurred while reading\n");
		fput(cur_fd);
		return read_ret;
	}*/
	/* 3. update f_pos
	 * This is similar to our offset counter .
	 * Need to update the position of the cursor
	 * to where it is currently pointing. So the read_ret
	 * actually returns the number of bytes read
	 */
	 cur_fd->f_pos = cur_fd->f_pos+read_ret;

	 /* 4. fput it */
	 /* - Decrement f_count.
	  * - If f_count == 0, call vput() and free it. */
	 fput(cur_fd);
	 dbg(DBG_PRINT, "(GRADING2B ): Successful Read of the file\n");
	 /* 5. return the number of bytes read, or an error */
	 /* not handling the error scenarios yet */
	 return read_ret;
}

/* Very similar to do_read.  Check f_mode to be sure the file is writable.  If
 * f_mode & FMODE_APPEND, do_lseek() to the end of the file, call the write
 * fs_op, and fput the file.  As always, be mindful of refcount leaks.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for writing.
 */
int
do_write(int fd, const void *buf, size_t nbytes)
{
	 if(fd>=NFILES||fd<0||curproc->p_files[fd]==NULL){
		dbg(DBG_PRINT, "(GRADING2B ): fd is not a valid file descriptor or is not open for writing.\n");
		return -EBADF;
	}
        file_t *reqFile;
        reqFile=fget(fd);  
        
        if(reqFile==NULL || reqFile->f_mode==1)   /* f_mode=1 means only read access, no write and no append also! */
        {
        	dbg(DBG_PRINT, "(GRADING2B ): fd is not a valid file descriptor or is not open for writing.\n");
          if(reqFile!=NULL) { fput(reqFile); }
          return -EBADF;
        }

        /*if(S_ISDIR(reqFile->f_vnode->vn_mode))   
        {
        dbg(DBG_PRINT, "fd is not a valid file descriptor or is not open for reading\n");
          fput(reqFile);  
          return -EISDIR;
        }*/
        
        /*if (reqFile->f_mode & O_APPEND)*/   /* We need to append buf to end of the file */
        if(reqFile->f_mode == (FMODE_WRITE|FMODE_READ|FMODE_APPEND) )
        {
           int lseekReturnCode = do_lseek(fd,0,SEEK_END);
	   /*if(lseekReturnCode < 0)
           {
             return lseekReturnCode; 
           }*/
        }

	int bytesWritten= reqFile->f_vnode->vn_ops->write( reqFile->f_vnode, reqFile->f_pos, buf, nbytes );

        /*if(bytesWritten<0)
        {
          dbg(DBG_PRINT," Bytes read less than zero.\n"); 
          fput(reqFile);
          return bytesWritten;
        }*/

        reqFile->f_pos+=bytesWritten;
        KASSERT( (S_ISCHR(reqFile->f_vnode->vn_mode)) ||
                                         (S_ISBLK(reqFile->f_vnode->vn_mode)) ||
                                         ((S_ISREG(reqFile->f_vnode->vn_mode)) && (reqFile->f_pos <= reqFile->f_vnode->vn_len)) );
        dbg(DBG_PRINT, "(GRADING2A 3.a): Succesfully wrote to file\n");
        
        fput(reqFile);
        return bytesWritten;
}

/*
 * Zero curproc->p_files[fd], and fput() the file. Return 0 on success
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't a valid open file descriptor.
 */
int
do_close(int fd)
{
	 if(fd>=NFILES||fd<0||curproc->p_files[fd]==NULL){
		dbg(DBG_PRINT, "(GRADING2B): fd isn't a valid open file descriptor.\n");
		return -EBADF;
	}

	file_t *reqFile;
        reqFile=fget(fd);  
        
        /*if(reqFile==NULL)
        {
          return -EBADF;
        }*/
        curproc->p_files[fd]=NULL;
        fput(reqFile);  /* this one for the fget() */
        fput(reqFile);    /* this actually makes it 0 (should make it 0, is no ref count errors are there) */
        dbg(DBG_PRINT, "(GRADING2B): Closed File Successfully.\n");
        return 0;
}

/* To dup a file:
 *      o fget(fd) to up fd's refcount
 *      o get_empty_fd()
 *      o point the new fd to the same file_t* as the given fd
 *      o return the new file descriptor
 *
 * Don't fput() the fd unless something goes wrong.  Since we are creating
 * another reference to the file_t*, we want to up the refcount.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't an open file descriptor.
 *      o EMFILE
 *        The process already has the maximum number of file descriptors open
 *        and tried to open a new one.
 */
int
do_dup(int fd)
{
	if(fd>=NFILES||fd<0||curproc->p_files[fd]==NULL){
		dbg(DBG_PRINT, "(GRADING2B): fd isn't an open file descriptor.\n");
		return -EBADF;
	}
	/*NOT_YET_IMPLEMENTED("VFS: do_dup");*/
	file_t *reqFile;
	reqFile=fget(fd);  
        
        /*if(reqFile==NULL)
        {
          return -EBADF;
        }*/
        
        int aNewFd = get_empty_fd(curproc);  /*function in fs/open.c */
        /*if (aNewFd==-EMFILE)
        {
          fput(reqFile);*/   /* Have to do --refcount because a duplicate failed (no space for a new fd) */
         /* return -EMFILE;
        }  */
        curproc->p_files[aNewFd]=reqFile;  /*Make the new fd entry of this process point to the same file_t. No need to do --refcount*/
        dbg(DBG_PRINT, "(GRADING2B): Duplication successful for FD : %d.\n",aNewFd);
        return aNewFd;
}

/* Same as do_dup, but insted of using get_empty_fd() to get the new fd,
 * they give it to us in 'nfd'.  If nfd is in use (and not the same as ofd)
 * do_close() it first.  Then return the new file descriptor.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        ofd isn't an open file descriptor, or nfd is out of the allowed
 *        range for file descriptors.
 */
int
do_dup2(int ofd, int nfd)
{
	if(ofd>=NFILES||ofd<0||curproc->p_files[ofd]==NULL){
		dbg(DBG_PRINT, "(GRADING2B): ofd isn't an open file descriptor, or nfd is out of the allowed range for file descriptors\n");
		return -EBADF;
	}

	if(nfd>=NFILES||nfd<0){
		dbg(DBG_PRINT, "(GRADING2B): ofd isn't an open file descriptor, or nfd is out of the allowed range for file descriptors\n");		
		return -EBADF;
	}

	/* Check if the new fd is not the same as the old fd */
	file_t *cur_file = NULL;
	if(nfd != ofd){
		/*If nfd is in use (and not the same as ofd)
		 * do_close() it first.*/
		if((cur_file=fget(nfd))!=NULL){
			/* close the new file descriptor*/
			fput(cur_file);
			do_close(nfd);
		}
		/* first get the file correspoding to the old descriptor*/
		cur_file = fget(ofd);

		/* set the current process's with
		 * the new file descriptor to point
		 * with  */
		curproc->p_files[nfd] = cur_file;

	}
	/* Return the new file descriptor */
	dbg(DBG_PRINT, "(GRADING2B): Duplication successful to New FD : %d.\n",nfd);
	return nfd;
}

/*
 * This routine creates a special file of the type specified by 'mode' at
 * the locatison specified by 'path'. 'mode' should be one of S_IFCHR or
 * S_IFBLK (you might note that mknod(2) normally allows one to create
 * regular files as well-- for simplicity this is not the case in Weenix).
 * 'devid', as you might expect, is the device identifier of the device
 * that the new special file should represent.
 *
 * You might use a combination of dir_namev, lookup, and the fs-specific
 * mknod (that is, the containing directory's 'mknod' vnode operation).
 * Return the result of the fs-specific mknod, or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        mode requested creation of something other than a device special
 *        file.
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mknod(const char *path, int mode, unsigned devid)
{
    /*
	 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
	 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
	 * vnode corresponding to "/s5fs/bin" in res_vnode.
	 */
	size_t namelen=0;
	const char *name=NULL;
	vnode_t *base = NULL;
	vnode_t *res_vnode =NULL;
	vnode_t *temp_vnode =NULL;

	/*if((mode!=S_IFCHR) && (mode!=S_IFBLK))
	{
		KASSERT(mode!=S_IFCHR&&mode!=S_IFBLK);
		dbg(DBG_PRINT,"(GRADING2D 3.b): Device Special files created in the wrong mode.\n");
		return -EINVAL;
	}*/
	/* We have to recursively keep getting the inode number corresponding to the
	 * directories mentioned in the path until we reach the inode of the
	 * drivers.*/
	int dir_namev_ret = dir_namev(path, &namelen, &name, base,&res_vnode);
	/*if(dir_namev_ret<0)
	{
		KASSERT(dir_namev_ret<0);
		dbg(DBG_PRINT,"(GRADING2D 3.b):File path has a problem\n");
		return dir_namev_ret;
	}*/

	/*if(!S_ISDIR(res_vnode->vn_mode))
	{
		KASSERT(!S_ISDIR(res_vnode->vn_mode));
		dbg(DBG_PRINT, "(GRADING2D 3.b): Some directory component is not a directory\n");
		vput(res_vnode);
		return -ENOTDIR;
	}*/

	/* Need to check res_vnode before we call this to avoid conflict */
	/* To check if the path exists */

	KASSERT(name!=NULL);
	dbg(DBG_PRINT, "(GRADING2B): The name component is present\n");
	int loookup_ret=lookup(res_vnode,name,namelen,&temp_vnode);
	/*if(0==loookup_ret)
	{
		KASSERT(0==loookup_ret);
		dbg(DBG_PRINT, "(GRADING2D 3.b): The path exists. \n");
		vput(res_vnode);
		vput(temp_vnode);
		return -EEXIST;
	}*/
	KASSERT(NULL!=res_vnode->vn_ops->mknod);
	dbg(DBG_PRINT, "(GRADING2A 3.b): The function pointer for mknod is not null\n");
	/* We simply have to call the inbuilt function for the ramfs implementation extensible to the
	 * s5fs */
	int mknod_ret =(res_vnode->vn_ops->mknod)(res_vnode,name,namelen,mode,devid);
	/*if(mknod_ret<0)
	{
		KASSERT(mknod_ret<0);
		dbg(DBG_PRINT,"(GRADING2D 3.b):Could not make the special node\n");
		vput(res_vnode);
		return mknod_ret;
	}*/
	/*dbg(DBG_PRINT,"(GRADING2D 3.b): Successful in making the special device node !!!\n");*/
	vput(res_vnode);
	dbg(DBG_PRINT, "(GRADING2B): Special File successfuly created\n");
    return mknod_ret;
}

/* Use dir_namev() to find the vnode of the dir we want to make the new
 * directory in.  Then use lookup() to make sure it doesn't already exist.
 * Finally call the dir's mkdir vn_ops. Return what it returns.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mkdir(const char *path)
{
	/*
	 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
	 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
	 * vnode corresponding to "/s5fs/bin" in res_vnode.
	 */
	size_t namelen=0;
	const char *name=NULL;
	vnode_t *base = NULL;
	vnode_t *res_vnode=NULL;
	vnode_t *temp_vnode=NULL;

	/* Calling the dir_namev() to find the vnode
	 * of the directory we want to make the new
	 * directory in */
	int dir_namev_ret = dir_namev(path, &namelen, &name, base,&res_vnode);
	if(dir_namev_ret<0)
	{
		KASSERT(dir_namev_ret<0);
		dbg(DBG_PRINT,"(GRADING2B): Directory path has a problem\n");
		return dir_namev_ret;
	}


	if(!S_ISDIR(res_vnode->vn_mode))
	{
		KASSERT(!S_ISDIR(res_vnode->vn_mode));
		dbg(DBG_PRINT, "(GRADING2B): Some directory component is not a directory\n");
		vput(res_vnode);
		return -ENOTDIR;
	}

	/* Need to check res_vnode before we call this to avoid conflict */
	/* To check if the path exists */

	/*KASSERT(*name!=NULL);*/
	dbg(DBG_PRINT, "(GRADING2C 1): The name component is present\n");

	int lookup_ret = lookup(res_vnode,name,namelen,&temp_vnode);
	if(0==lookup_ret)
	{
		KASSERT(0==lookup_ret);
		dbg(DBG_PRINT, "(GRADING2B): The path exists. Would not be able to create the directory.\n");
		vput(res_vnode);
		vput(temp_vnode);
		return -EEXIST;
	}
	else if(lookup_ret != -ENOENT)
	{
		vput(res_vnode);
		return lookup_ret;
	}

	/*vput(temp_vnode);*/
	KASSERT(NULL!=res_vnode->vn_ops->mkdir);
	dbg(DBG_PRINT, "(GRADING2A 3.c): The function pointer for mkdir is not null\n");
	int mkdir_ret = (res_vnode->vn_ops->mkdir)(res_vnode,name,namelen);

	/*if(mkdir_ret<0)
	{
		KASSERT(mkdir_ret<0);
		vput(res_vnode);
		dbg(DBG_PRINT,"(GRADING2D 3.b):Could not make the directory\n");
		return mkdir_ret;
	}*/

	/*dbg(DBG_PRINT, "(GRADING2D 3.c): Directory is made at %s\n",path);*/
	vput(res_vnode);
	dbg(DBG_PRINT, "(GRADING2B): Special directory created.\n");
	return mkdir_ret;
}

/* Use dir_namev() to find the vnode of the directory containing the dir to be
 * removed. Then call the containing dir's rmdir v_op.  The rmdir v_op will
 * return an error if the dir to be removed does not exist or is not empty, so
 * you don't need to worry about that here. Return the value of the v_op,
 * or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        path has "." as its final component.
 *      o ENOTEMPTY
 *        path has ".." as its final component.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
	int
do_rmdir(const char *path)
{
	/*
	 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
	 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
	 * vnode corresponding to "/s5fs/bin" in res_vnode.
	 */
	size_t namelen=0;
	const char *name=NULL;
	vnode_t *base = NULL;
	vnode_t *res_vnode=NULL;
	vnode_t *temp_vnode=NULL;

	/* Calling the dir_namev() to find the vnode
	 * of the directory we want to make the new
	 * directory in */
	int dir_namev_ret = dir_namev(path, &namelen, &name, base,&res_vnode);
	
	if(dir_namev_ret<0)
	{
		KASSERT(dir_namev_ret<0);
		dbg(DBG_PRINT, "(GRADING2B): Some directory component does not exist\n");
		return dir_namev_ret;
	}
	else
	{
		if(!S_ISDIR(res_vnode->vn_mode))
		{
			KASSERT(!S_ISDIR(res_vnode->vn_mode));
			dbg(DBG_PRINT, "(GRADING2B): Some directory component is not a directory\n");
			vput(res_vnode);
			return -ENOTDIR;
		}
	}

	if( (*(name+(strlen(name)-1))=='.') && (*(name+(strlen(name)-2))=='.')  ) 
		/*if(strcmp(*name,"..")==0)*/
	{
		dbg(DBG_PRINT,"(GRADING2B): Path had '..' as final component.\n");
		vput(res_vnode);
		return -ENOTEMPTY;
	}
	
	if(*(name+(strlen(name)-1))=='.')
		/*if(strcmp(*name,".")==0 || namelen == 0)*/ 
	{
		dbg(DBG_PRINT,"(GRADING2B): Path has '.' as its final component.\n");
		vput(res_vnode);
		return -EINVAL;
	}
	
	/* Need to check res_vnode before we call this to avoid conflict */
	/*if(*name!=NULL)
	{*/
		/* To check if the path exists */
		/*KASSERT(*name!=NULL); 
		dbg(DBG_PRINT, "(GRADING2D 3.d): The name component is present\n");*/
	int lookup_ret=lookup(res_vnode,name,namelen,&temp_vnode);
	if(lookup_ret<0)
	{
		KASSERT(lookup_ret<0);
		dbg(DBG_PRINT, "(GRADING2B): Lookup failed.\n");
		vput(res_vnode);
		return lookup_ret;
	}
	KASSERT(NULL!=res_vnode->vn_ops->rmdir);
	dbg(DBG_PRINT, "(GRADING2A 3.d): The function call exists for remove directory.\n");
	int rmdir_ret=(res_vnode->vn_ops->rmdir)(res_vnode,name,namelen);
	vput(temp_vnode);
	vput(res_vnode);
	dbg(DBG_PRINT, "(GRADING2B): Directory successfully removed.\n");
	return rmdir_ret;
}

/*
 * Same as do_rmdir, but for files.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EISDIR
 *        path refers to a directory.
 *      o ENOENT
 *        A component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_unlink(const char *path)
{
	/* For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
	 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
	 * vnode corresponding to "/s5fs/bin" in res_vnode.
	 */
	size_t namelen=0;
	const char *name=NULL;
	vnode_t *base = NULL;
	vnode_t *res_vnode=NULL;
	vnode_t *temp_vnode=NULL;

	KASSERT(path);
	/*if(path==NULL){
		dbg(DBG_PRINT, "(GRADING2B): No Entry found.\n");
		return -ENOENT;
	}*/
	/* Calling the dir_namev() to find the vnode
	 * of the directory we want to make the new
	 * directory in */
	int dir_namev_ret = dir_namev(path, &namelen, &name, base,&res_vnode);
	/*if(dir_namev_ret<0)
	{
		KASSERT(dir_namev_ret<0);
		dbg(DBG_PRINT,"(GRADING2D 3.e):File path has a problem\n");
		return dir_namev_ret;
	}*/

	/*if(!S_ISDIR(res_vnode->vn_mode))
	{
		KASSERT(!S_ISDIR(res_vnode->vn_mode));
		dbg(DBG_PRINT, "(GRADING2D 3.e): The path does not lead to a directory.\n");
		vput(res_vnode);
		return -ENOTDIR;
	}*/
	
	int lookup_ret=lookup(res_vnode,name,namelen,&temp_vnode);
	if(lookup_ret<0)
	{
		KASSERT(lookup_ret<0);
		dbg(DBG_PRINT, "(GRADING2B): The path does not exist.\n");
		vput(res_vnode);
		return lookup_ret;
	}

	/*vput(temp_vnode);
	}*/

	if(S_ISDIR(temp_vnode->vn_mode))
	{
		KASSERT(S_ISDIR(temp_vnode->vn_mode));
		vput(temp_vnode);
		vput(res_vnode);
		dbg(DBG_PRINT, "(GRADING2B): Trying to remove directory.\n");
		return -EISDIR;
	}

	KASSERT(NULL!=res_vnode->vn_ops->unlink);
	dbg(DBG_PRINT, "(GRADING2A 3.e): The function call for unlink exists.\n");

	int unlink_ret=(res_vnode->vn_ops->unlink)(res_vnode,name,namelen);

	/*if(unlink_ret<0){
		KASSERT(unlink_ret<0);
		dbg(DBG_PRINT, "(GRADING2A 3.e): The unlinking did not work.\n");
		vput(temp_vnode);
		vput(res_vnode);
		return unlink_ret;
	}*/
	vput(temp_vnode);
	vput(res_vnode);
	dbg(DBG_PRINT, "(GRADING2B): File removed Successfully.\n");
	return unlink_ret;
}

/* To link:
 *      o open_namev(from)
 *      o dir_namev(to)
 *      o call the destination dir's (to) link vn_ops.
 *      o return the result of link, or an error
 *
 * Remember to vput the vnodes returned from open_namev and dir_namev.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        to already exists.
 *      o ENOENT
 *        A directory component in from or to does not exist.
 *      o ENOTDIR
 *        A component used as a directory in from or to is not, in fact, a
 *        directory.
 *      o ENAMETOOLONG
 *        A component of from or to was too long.
 *      o EISDIR
 *        from is a directory.
 */
int
do_link(const char *from, const char *to)
{
	NOT_YET_IMPLEMENTED("Link ");
	return -1;
	/*size_t namelen=0;
	const char *name=NULL;
	vnode_t *base = NULL;
	vnode_t *res_vnode_to=NULL;
	vnode_t *res_vnode_from=NULL;
	vnode_t *temp_vnode=NULL;

	KASSERT(from);
	KASSERT(to);

	int from_namev_ret=open_namev(from,NULL,&res_vnode_from,NULL);
	if(from_namev_ret<0)
	{
		KASSERT(from_namev_ret<0);
		dbg(DBG_PRINT,"(GRADING2D 3.g): From component has failed.\n");
		return from_namev_ret;
	}
	KASSERT(res_vnode_from);
	if(S_ISDIR(res_vnode_from->vn_mode))
	{
		KASSERT(S_ISDIR(res_vnode_from->vn_mode));
		dbg(DBG_PRINT, "(GRADING2D 3.g): From is a directory.\n");
		vput(res_vnode_from);
		return -EISDIR;
	}

	int to_namev_ret=dir_namev(to, &namelen,&name,NULL,&res_vnode_to);
	if(to_namev_ret<0)
	{*/
	/* Here we have to check if the vnode from has occurred successfully.
	  * If yes then simply vput the from */
	/*	KASSERT(to_namev_ret<0);
		dbg(DBG_PRINT,"(GRADING2D 3.g): To component has failed.\n");

		vput(res_vnode_from);
		return to_namev_ret;
	}
	KASSERT(res_vnode_to);

	if(!S_ISDIR(res_vnode_to->vn_mode))
	{
		KASSERT(!S_ISDIR(res_vnode_to->vn_mode));
		dbg(DBG_PRINT, "(GRADING2D 3.g): Some directory component is not a directory\n");
		vput(res_vnode_to);
		vput(res_vnode_from);
		return -ENOTDIR;
	}*/

	/* Need to check res_vnode before we call this to avoid conflict */
	/* To check if the path exists */
	/*KASSERT(name!=NULL);
	dbg(DBG_PRINT, "(GRADING2D 3.g): The name component is present\n");

	int loookup_ret=lookup(res_vnode_to,name,namelen,&temp_vnode);
	if(0==loookup_ret)
	{
		KASSERT(0==loookup_ret);
		dbg(DBG_PRINT, "(GRADING2D 3.g): The path exists. Would not be able to create the directory.\n");
		vput(res_vnode_to);
		vput(res_vnode_from);
		vput(temp_vnode);
		return -EEXIST;
	}

	KASSERT(res_vnode_to->vn_ops->link);
	int link_ret=(res_vnode_to->vn_ops->link)(res_vnode_from,res_vnode_to,name,namelen);
	vput(res_vnode_to);
	vput(res_vnode_from);

	if(link_ret<0){
		dbg(DBG_PRINT, "(GRADING2D 3.g): There is a problem in linking.\n");
		return link_ret;
	}

	dbg(DBG_PRINT, "(GRADING2D 3.g): Successfully linked.\n");
	return link_ret;*/
}

/*      o link newname to oldname
 *      o unlink oldname
 *      o return the value of unlink, or an error
 *
 * Note that this does not provide the same behavior as the
 * Linux system call (if unlink fails then two links to the
 * file could exist).
 */
	int
do_rename(const char *oldname, const char *newname)
{
	/*if(oldname == NULL)
	{
		KASSERT(oldname==NULL);
		dbg(DBG_PRINT,"(GRADING2D 3.h): Old Name from path does not exist.\n");
		return -ENOENT;
	}
	if(newname == NULL)
	{
		KASSERT(newname==NULL);
		dbg(DBG_PRINT,"(GRADING2D 3.h): New Name path does not exist.\n");
		return -ENOENT;
	}

	if(strlen(oldname)>MAXPATHLEN){
		KASSERT(strlen(oldname)>MAXPATHLEN);
		dbg(DBG_PRINT,"(GRADING2D 3.h): Oldname component of path was too long.\n");
		return -ENAMETOOLONG;
	}
	if(strlen(newname)>MAXPATHLEN){
		KASSERT(strlen(newname)>MAXPATHLEN);
		dbg(DBG_PRINT,"(GRADING2D 3.h): New Name component of path was too long.\n");
		return -ENAMETOOLONG;
	}*/
	/* link newname to oldname */
	/*int link_ret = do_link(oldname,newname);
	if(link_ret<0){
		KASSERT(link_ret<0);
		dbg(DBG_PRINT,"(GRADING2D 3.h): Linking failed.\n");
		return link_ret;
	}*/
	/*unlink oldname*/
	/*int unlink_ret = do_unlink(oldname);
	if(unlink_ret<0){
		KASSERT(unlink_ret<0);
		dbg(DBG_PRINT,"(GRADING2D 3.h): UnLinking failed.\n");
		return unlink_ret;
	}
	dbg(DBG_PRINT,"(GRADING2D 3.h): Linking Successful.\n");
	return unlink_ret;*/
	NOT_YET_IMPLEMENTED("Renaming");
	return -1;
}

/* Make the named directory the current process's cwd (current working
 * directory).  Don't forget to down the refcount to the old cwd (vput()) and
 * up the refcount to the new cwd (open_namev() or vget()). Return 0 on
 * success.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        path does not exist.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 *      o ENOTDIR
 *        A component of path is not a directory.
 */
int
do_chdir(const char *path)
{
	size_t namelen=0;
	const char *name=NULL;
	vnode_t *base = NULL;
	vnode_t *res_vnode=NULL;

	KASSERT(path);

	int namev_ret = open_namev(path, 0, &res_vnode, NULL);

	if(namev_ret<0)
	{
		KASSERT(namev_ret<0);
		dbg(DBG_PRINT,"(GRADING2B): The path does not exist.\n");
		return namev_ret;
	}

	if(!S_ISDIR(res_vnode->vn_mode))
	{
		KASSERT(!S_ISDIR(res_vnode->vn_mode));
		dbg(DBG_PRINT, "(GRADING2B): The path is not to a directory.\n");
		vput(res_vnode);
		return -ENOTDIR;
	}

	KASSERT(res_vnode != NULL);

	/* Don't forget to down the refcount to the old cwd (vput()) and
	 * up the refcount to the new cwd (open_namev() or vget()).*/
	KASSERT(curproc != NULL);

	vput(curproc->p_cwd);
	curproc->p_cwd=res_vnode;

	/* Might need to increase the ref_count, but I think it should instead
	 * happen inside lookup which gets called by open_namev.*/ 
	/*vget(res_vnode->vn_fs,res_vnode->vn_vno);*/

	dbg(DBG_PRINT, "(GRADING2B): Successfully changed directory.\n");
	return 0;
}

/* Call the readdir fs_op on the given fd, filling in the given dirent_t*.
 * If the readdir fs_op is successful, it will return a positive value which
 * is the number of bytes copied to the dirent_t.  You need to increment the
 * file_t's f_pos by this amount.  As always, be aware of refcounts, check
 * the return value of the fget and the virtual function, and be sure the
 * virtual function exists (is not null) before calling it.
 *
 * Return either 0 or sizeof(dirent_t), or -errno.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        Invalid file descriptor fd.
 *      o ENOTDIR
 *        File descriptor does not refer to a directory.
 */
int
do_getdent(int fd, struct dirent *dirp)
{
	if(fd>=NFILES||fd<0||curproc->p_files[fd]==NULL){
		dbg(DBG_PRINT, "(GRADING2B): Invalid file descriptor fd.\n");
		return -EBADF;
	}
	/* Call the readdir fs_op on the given fd, filling in the given dirent_t*.*/
	file_t *cur_f = fget(fd);
	/* If the readdir fs_op is successful, it will return a positive value which
	 * is the number of bytes copied to the dirent_t.  You need to increment the
	 * file_t's f_pos by this amount.*/

	if(!S_ISDIR(cur_f->f_vnode->vn_mode))
	{
		KASSERT(!S_ISDIR(cur_f->f_vnode->vn_mode));
		dbg(DBG_PRINT, "(GRADING2B): File descriptor does not refer to a directory.\n");
		fput(cur_f);
		return -ENOTDIR;
	}

	KASSERT(cur_f->f_vnode->vn_ops->readdir);
	int readdir_ret=(cur_f->f_vnode->vn_ops->readdir)(cur_f->f_vnode,cur_f->f_pos,dirp);

	/* Return either 0 or sizeof(dirent_t), or -errno. */
	/*if(readdir_ret<0)
	{
		fput(cur_f);
		return readdir_ret;
	}
	else*/
        if(readdir_ret == 0)
	{
	        dbg(DBG_PRINT, "(GRADING2B): Reading the directory entry successfully.\n");
		fput(cur_f);
		return 0;
	}
	else
	{
	        dbg(DBG_PRINT, "(GRADING2B): Reading the directory entry failed.\n");
	        cur_f->f_pos+=readdir_ret;
		fput(cur_f);
		return sizeof(*dirp);
	}
}

/*
 * Modify f_pos according to offset and whence.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not an open file descriptor.
 *      o EINVAL
 *        whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting
 *        file offset would be negative.
 */
int
do_lseek(int fd, int offset, int whence)
{
	if(fd>=NFILES||fd<0||curproc->p_files[fd]==NULL){
		dbg(DBG_PRINT, "(GRADING2B): fd is not an open file descriptor.\n");
		return -EBADF;
	}

	file_t *cur_file = fget(fd);

        /*if(cur_file==NULL) 
        {
          if(cur_file!=NULL) { 
		fput(cur_file); 
          }
          return -EBADF;
        }*/

	if(whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END){
	         fput(cur_file);
		dbg(DBG_PRINT, "(GRADING2B): whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting file offset would be negative.\n");
		return -EINVAL;
	}

	/* Modify f_pos according to offset and whence. */
	/* Now there can be 3 conditions :
	 * SEEK_SET : So offset is from the begining position
	 */
	if(whence == SEEK_SET)
	{
		if(offset < 0 ){
				dbg(DBG_PRINT, "(GRADING2B): whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting file offset would be negative.\n");
				fput(cur_file);
				return -EINVAL;
			}
		cur_file->f_pos = offset;
		fput(cur_file);
	}
	else if(whence == SEEK_CUR){
		if(cur_file->f_vnode->vn_len + offset < 0 ){
		dbg(DBG_PRINT, "(GRADING2B): whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting file offset would be negative.\n");
				fput(cur_file);
				return -EINVAL;
		}
		cur_file->f_pos = cur_file->f_pos + offset;
		fput(cur_file);
	}
	else if(whence == SEEK_END){
		if(cur_file->f_vnode->vn_len + offset < 0 ){
		dbg(DBG_PRINT, "(GRADING2B): whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting file offset would be negative.\n");
				fput(cur_file);
				return -EINVAL;
		}
		cur_file->f_pos = cur_file->f_vnode->vn_len + offset;
		fput(cur_file);
	}
	dbg(DBG_PRINT, "(GRADING2B): Successful seek \n");
	return cur_file->f_pos;
}

/*
 * Find the vnode associated with the path, and call the stat() vnode operation.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        A component of path does not exist.
 *      o ENOTDIR
 *        A component of the path prefix of path is not a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
	int
do_stat(const char *path, struct stat *buf)
{
	if(*path == '\0')
	{
		KASSERT(*path == '\0');
		dbg(DBG_PRINT,"(GRADING2B): Path component empty!\n");
		return -EINVAL;
	}

	size_t namelen=0;
	const char *name=NULL;
	vnode_t *temp_vnode = NULL;
	vnode_t *res_vnode=NULL;

	KASSERT(path);
	KASSERT(buf);

	int namev_ret = dir_namev(path,&namelen,&name,NULL,&res_vnode);
	/*if(namev_ret<0){
		KASSERT(namev_ret<0);
		dbg(DBG_PRINT,"(GRADING2D 3.f): Path component has failed.\n");*/
                  /* this should take care of ENAMETOOLONG*/
	/*	return namev_ret;
	}*/

	/*if(res_vnode == NULL)*/   /*because dirnamev doesnt return ENOENT*/
	/*{
	  dbg(DBG_PRINT," Directory component in path does not exist\n");
	  return -ENOENT;
	}*/
	/*if(!S_ISDIR(res_vnode->vn_mode))
        {
          dbg(DBG_PRINT,"A component of the path prefix of path is not a directory.\n");
          vput(res_vnode);
          return -ENOTDIR;
        }*/
        
        /* All return codes taken care off */
        namev_ret =  lookup(res_vnode,name,namelen,&temp_vnode);
        
        if (namev_ret<0)
        {
          dbg(DBG_PRINT,"(GRADING2B):Lookup failed, name is not in directory\n");
          vput(res_vnode);
          return namev_ret;
        }

	KASSERT(res_vnode->vn_ops->stat);
	dbg(DBG_PRINT, "(GRADING2A 3.f): The stat function exists !!!.\n");
	int stat_ret=(res_vnode->vn_ops->stat)(temp_vnode,buf);
	vput(res_vnode);
	vput(temp_vnode);
	dbg(DBG_PRINT,"(GRADING2B): Successfully Found the vnode associated with the path");
	return stat_ret;
}

#ifdef __MOUNTING__
/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutely sure your Weenix is perfect.
 *
 * This is the syscall entry point into vfs for mounting. You will need to
 * create the fs_t struct and populate its fs_dev and fs_type fields before
 * calling vfs's mountfunc(). mountfunc() will use the fields you populated
 * in order to determine which underlying filesystem's mount function should
 * be run, then it will finish setting up the fs_t struct. At this point you
 * have a fully functioning file system, however it is not mounted on the
 * virtual file system, you will need to call vfs_mount to do this.
 *
 * There are lots of things which can go wrong here. Make sure you have good
 * error handling. Remember the fs_dev and fs_type buffers have limited size
 * so you should not write arbitrary length strings to them.
 */
	int
do_mount(const char *source, const char *target, const char *type)
{
	NOT_YET_IMPLEMENTED("MOUNTING: do_mount");
	return -EINVAL;
}

/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutley sure your Weenix is perfect.
 *
 * This function delegates all of the real work to vfs_umount. You should not worry
 * about freeing the fs_t struct here, that is done in vfs_umount. All this function
 * does is figure out which file system to pass to vfs_umount and do good error
 * checking.
 */
int
do_umount(const char *target)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_umount");
        return -EINVAL;
}
#endif
