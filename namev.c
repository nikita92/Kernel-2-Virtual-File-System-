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
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"

/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function, but you may want to special case
 * "." and/or ".." here depnding on your implementation.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
	KASSERT(NULL != dir);
	dbg(DBG_PRINT,"(GRADING2A 2.a): Directory is not null\n");
	KASSERT(NULL != name);
	dbg(DBG_PRINT,"(GRADING2A 2.a): Name is not null\n");
	KASSERT(NULL != result);
	dbg(DBG_PRINT,"(GRADING2A 2.a): Result is not null\n");
	/*if ((!S_ISDIR(dir->vn_mode)))
	{
		dbg(DBG_PRINT,"(GRADING2D 2.a): Not possible to lookup\n");
		return -ENOTDIR;
	}*/
	if (len > NAME_LEN)
	{
		dbg(DBG_PRINT,"(GRADING2D 2.a): Name is too long.\n");
		return -ENAMETOOLONG;
	}
	/* First checking for the special case of "." .
	 * I am not sure how to handle the ".." type files.
	 * Since in this case we need to get the vnode of the parent directory.
	 * But I think this is handled by default. If there is a ".." then we
	 * will not increase the reference to the current directory. */
	if(strcmp(name,".")==0 || len==0)
	{
		dbg(DBG_PRINT,"(GRADING2D 2.a) : Current directory : %s \n",name);
		vref(dir);
		*result=dir;
		return 0;
	}
	
	/*Most important part !!! Here I am calling the lookup specific to the filesystem we are using .
	It doesnt matter what type of filesystem we use - ramfs or s5fs*/
	int lookup_ret = (dir->vn_ops->lookup)(dir,name,len,result);
	if(lookup_ret<0)
	{
		dbg(DBG_PRINT,"(GRADING2D 2.a) : The requested file does not exist : %s \n",name);
		return -ENOENT;
	}
	/*dbg(DBG_PRINT,"(GRADING2D 2.a) : Successful lookup of the requested directory : %s \n",name);*/
	/*vref(dir);*/
	/*result=dir;*/
    return lookup_ret;
}


/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name,
          vnode_t *base, vnode_t **res_vnode)
{
	KASSERT(NULL != pathname);
	dbg(DBG_PRINT,"(GRADING2A 2.b): pathname is not null\n");
	KASSERT(NULL != namelen);
	dbg(DBG_PRINT,"(GRADING2A 2.b): namelen is not null\n");
	KASSERT(NULL != name);
	dbg(DBG_PRINT,"(GRADING2A 2.b): name is not null\n");
	KASSERT(NULL != res_vnode);
	dbg(DBG_PRINT,"(GRADING2A 2.b): res_vnode is not null\n"); 
	

	/*if(strlen(pathname)>MAXPATHLEN)
	{
		dbg(DBG_PRINT,"(GRADING2D 2.b) : Path length is too long\n");
		return -ENAMETOOLONG;
	}*/

	/*dbg(DBG_PRINT,"(GRADING2D 2.b) : pathname,namelen,name,res_vnode are not null\n");*/
        char *tempPathname = (char *)pathname;
	vnode_t *vnode;
	vnode_t *ret_result;
	char *finalPathName=(char *)pathname + strlen(pathname);
	if(pathname[0]=='/')
	{
		vnode=vfs_root_vn;

		tempPathname++;

		dbg(DBG_PRINT,"(GRADING2D 2.b) : The requested directory starts with a root :  \n");
	}else if(base == NULL)
	{
		vnode = curproc->p_cwd;
	}
	/*else
	{
		vnode = base;
	}*/

	vref(vnode);

	while(*tempPathname == '/')
		tempPathname++;

	char * currentPathAccess = strchr(tempPathname,'/');
	while(currentPathAccess)
	{
		int tempValue = lookup(vnode,tempPathname,currentPathAccess - tempPathname,&ret_result);

		if(tempValue<0)
		{
			vput(vnode);
			return tempValue;
		}

		vput(vnode);	/* ADDED */
		vnode=ret_result;   /* ADDED */

		/* This would mean that the path ended with a '/' */
		KASSERT(currentPathAccess != finalPathName);

		tempPathname = currentPathAccess + 1;

		while(*tempPathname == '/')
			tempPathname++;

		currentPathAccess = strchr(tempPathname,'/');
		
	}

	KASSERT(NULL != vnode);
	dbg(DBG_PRINT,"(GRADING2A 2.b) : The vnode is not null\n");
	*res_vnode = vnode;
	*namelen = (finalPathName-tempPathname);
	*name = (char*)tempPathname;
	return 0;
}

/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fnctl.h>.  If the O_CREAT flag is specified, and the file does
 * not exist call create() in the parent directory vnode.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
	size_t namelen=0;
	int create_ret = 0;
	const char *name=NULL;
	vnode_t *temp_vnode =NULL;
	/* We have to recursively keep getting the inode number corresponding to the
	 * directories mentioned in the path until we reach the inode of the
	 * drivers.*/
	int dir_namev_ret = dir_namev(pathname, &namelen, &name, base,&temp_vnode);
	
	KASSERT(pathname);

	if(dir_namev_ret<0)
	{
		KASSERT(dir_namev_ret<0);
		dbg(DBG_PRINT,"(GRADING2D 2.c):File path has a problem\n");
		return dir_namev_ret;
	}

	if(!S_ISDIR(temp_vnode->vn_mode))
	{
		KASSERT(!S_ISDIR(temp_vnode->vn_mode));
		dbg(DBG_PRINT, "(GRADING2D 2.c): Some directory component is not a directory\n");
		vput(temp_vnode);
		return -ENOTDIR;
	}

	/*KASSERT(*name != NULL);*/
	/* Need to check res_vnode before we call this to avoid conflict */
		/* To check if the path exists */
	/*dbg(DBG_PRINT, "(GRADING2D 2.c): The name component is present\n");*/
	int lookup_ret = lookup(temp_vnode,name,namelen,res_vnode);
	if(lookup_ret<0)
	{
		if((flag & O_CREAT) && (lookup_ret==-ENOENT))
		{
			KASSERT(NULL != temp_vnode->vn_ops->create);
			dbg(DBG_PRINT, "(GRADING2A 2.c): The create fs_ops exists for the node\n");
			create_ret = (temp_vnode->vn_ops->create)(temp_vnode, name,
					namelen, res_vnode);

			if(create_ret<0)
			{
				dbg(DBG_PRINT, "(GRADING2D 2.c): The path exists. \n");
				vput(temp_vnode);
				return create_ret;
			}
		}
		else
		{
			vput(temp_vnode);
			return 	lookup_ret;					
		}
	}
	vput(temp_vnode);
	/*dbg(DBG_PRINT, "(GRADING2D 2.c): Successful Creation . \n");*/
	return 0;
}

#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
	int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
	NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
	return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
