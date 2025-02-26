/*
 * Copyright (c) 1998-2014 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2014 Stony Brook University
 * Copyright (c) 2003-2014 The Research Foundation of SUNY
 *
 * This progr     is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "amfs.h"

static int amfs_create(struct inode *dir, struct dentry *dentry,
			 umode_t mode, bool want_excl)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_create(lower_parent_dentry->d_inode, lower_dentry, mode,
			 want_excl);
	if (err)
		goto out;
	err = amfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, amfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

out:
	unlock_dir(lower_parent_dentry);
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int amfs_link(struct dentry *old_dentry, struct inode *dir,
		       struct dentry *new_dentry)
{

	struct dentry *lower_old_dentry;
	struct dentry *lower_new_dentry;
	struct dentry *lower_dir_dentry;
	u64 file_size_save;
	int err;
	struct path lower_old_path, lower_new_path;

	char *xattr_value = (char*)kmalloc(3,GFP_KERNEL);

	/* if file is marked as bad */
	if(vfs_getxattr(old_dentry, (const char *)&"security.bad", (void *)xattr_value, 3)>0 && memcmp(xattr_value,&"yes",3)==0){
		printk("amfs:inode.c, amfs_link:bad file found\n");
		err = -EACCES;
		kfree(xattr_value);
		goto out_err;
	}

	file_size_save = i_size_read(old_dentry->d_inode);
	amfs_get_lower_path(old_dentry, &lower_old_path);
	amfs_get_lower_path(new_dentry, &lower_new_path);
	lower_old_dentry = lower_old_path.dentry;
	lower_new_dentry = lower_new_path.dentry;
	lower_dir_dentry = lock_parent(lower_new_dentry);

	err = vfs_link(lower_old_dentry, lower_dir_dentry->d_inode,
		       lower_new_dentry, NULL);
	if (err || !lower_new_dentry->d_inode)
		goto out;

	err = amfs_interpose(new_dentry, dir->i_sb, &lower_new_path);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, lower_new_dentry->d_inode);
	fsstack_copy_inode_size(dir, lower_new_dentry->d_inode);
	set_nlink(old_dentry->d_inode,
		  amfs_lower_inode(old_dentry->d_inode)->i_nlink);
	i_size_write(new_dentry->d_inode, file_size_save);
out:
	unlock_dir(lower_dir_dentry);
	amfs_put_lower_path(old_dentry, &lower_old_path);
	amfs_put_lower_path(new_dentry, &lower_new_path);
out_err:
	return err;
}

static int amfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int err;
	struct dentry *lower_dentry;
	struct inode *lower_dir_inode = amfs_lower_inode(dir);
	struct dentry *lower_dir_dentry;
	struct path lower_path;

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	dget(lower_dentry);
	lower_dir_dentry = lock_parent(lower_dentry);

	err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);

	/*
	 * Note: unlinking on top of NFS can cause silly-ren    ed files.
	 * Trying to delete such files results in EBUSY from NFS
	 * below.  Silly-renamed files will get deleted by NFS later on, so
	 * we just need to detect them here and treat such EBUSY errors as
	 * if the upper file was successfully deleted.
	 */
	if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
		err = 0;
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, lower_dir_inode);
	fsstack_copy_inode_size(dir, lower_dir_inode);
	set_nlink(dentry->d_inode,
		  amfs_lower_inode(dentry->d_inode)->i_nlink);
	dentry->d_inode->i_ctime = dir->i_ctime;
	d_drop(dentry); /* this is needed, else LTP fails (VFS won't do it) */
out:
	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int amfs_symlink(struct inode *dir, struct dentry *dentry,
			  const char *symname)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;

	char *xattr_value = (char*)kmalloc(3,GFP_KERNEL);

	/* if file is marked as bad */
	if(vfs_getxattr(dentry, (const char *)&"security.bad", (void *)xattr_value, 3)>0 && memcmp(xattr_value,&"yes",3)==0){
		printk("amfs:inode.c, amfs_symlink:bad file found\n");
		err = -EACCES;
		kfree(xattr_value);
		goto out;
	}

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_symlink(lower_parent_dentry->d_inode, lower_dentry, symname);
	if (err)
		goto out;
	err = amfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, amfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

out:
	unlock_dir(lower_parent_dentry);
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int amfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_mkdir(lower_parent_dentry->d_inode, lower_dentry, mode);
	if (err)
		goto out;

	err = amfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err)
		goto out;

	fsstack_copy_attr_times(dir, amfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);
	/* update number of links on parent directory */
	set_nlink(dir, amfs_lower_inode(dir)->i_nlink);

out:
	unlock_dir(lower_parent_dentry);
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int amfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry;
	int err;
	struct path lower_path;

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_dir_dentry = lock_parent(lower_dentry);

	err = vfs_rmdir(lower_dir_dentry->d_inode, lower_dentry);
	if (err)
		goto out;

	d_drop(dentry);	/* drop our dentry on success (why not VFS's job?) */
	if (dentry->d_inode)
		clear_nlink(dentry->d_inode);
	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
	fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
	set_nlink(dir, lower_dir_dentry->d_inode->i_nlink);

out:
	unlock_dir(lower_dir_dentry);
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int amfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode,
			dev_t dev)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_mknod(lower_parent_dentry->d_inode, lower_dentry, mode, dev);
	if (err)
		goto out;

	err = amfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, amfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

out:
	unlock_dir(lower_parent_dentry);
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

/*
 * The locking rules in amfs_rename are complex.  We could use a simpler
 * superblock-level name-space lock for renames and copy-ups.
 */
static int amfs_rename(struct inode *old_dir, struct dentry *old_dentry,
			 struct inode *new_dir, struct dentry *new_dentry)
{
	int err = 0;
	struct dentry *lower_old_dentry = NULL;
	struct dentry *lower_new_dentry = NULL;
	struct dentry *lower_old_dir_dentry = NULL;
	struct dentry *lower_new_dir_dentry = NULL;
	struct dentry *trap = NULL;
	struct path lower_old_path, lower_new_path;

	char *xattr_value = (char*)kmalloc(3,GFP_KERNEL);
	/* if file is marked as bad */
	if(vfs_getxattr(old_dentry, (const char *)&"security.bad", (void *)xattr_value, 3)>0 && memcmp(xattr_value,&"yes",3)==0){
		printk("amfs:inode.c, amfs_rename:bad file found\n");
		err = -EACCES;
		kfree(xattr_value);
		goto out;
	}

	amfs_get_lower_path(old_dentry, &lower_old_path);
	amfs_get_lower_path(new_dentry, &lower_new_path);
	lower_old_dentry = lower_old_path.dentry;
	lower_new_dentry = lower_new_path.dentry;
	lower_old_dir_dentry = dget_parent(lower_old_dentry);
	lower_new_dir_dentry = dget_parent(lower_new_dentry);

	trap = lock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	/* source should not be ancestor of target */
	if (trap == lower_old_dentry) {
		err = -EINVAL;
		goto out;
	}
	/* target should not be ancestor of source */
	if (trap == lower_new_dentry) {
		err = -ENOTEMPTY;
		goto out;
	}

	err = vfs_rename(lower_old_dir_dentry->d_inode, lower_old_dentry,
			 lower_new_dir_dentry->d_inode, lower_new_dentry,
			 NULL, 0);
	if (err)
		goto out;

	fsstack_copy_attr_all(new_dir, lower_new_dir_dentry->d_inode);
	fsstack_copy_inode_size(new_dir, lower_new_dir_dentry->d_inode);
	if (new_dir != old_dir) {
		fsstack_copy_attr_all(old_dir,
				      lower_old_dir_dentry->d_inode);
		fsstack_copy_inode_size(old_dir,
					lower_old_dir_dentry->d_inode);
	}

out:
	unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	dput(lower_old_dir_dentry);
	dput(lower_new_dir_dentry);
	amfs_put_lower_path(old_dentry, &lower_old_path);
	amfs_put_lower_path(new_dentry, &lower_new_path);
	return err;
}

static int amfs_readlink(struct dentry *dentry, char __user *buf, int bufsiz)
{
	int err;
	struct dentry *lower_dentry;
	struct path lower_path;

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	if (!lower_dentry->d_inode->i_op ||
	    !lower_dentry->d_inode->i_op->readlink) {
		err = -EINVAL;
		goto out;
	}

	err = lower_dentry->d_inode->i_op->readlink(lower_dentry,
						    buf, bufsiz);
	if (err < 0)
		goto out;
	fsstack_copy_attr_atime(dentry->d_inode, lower_dentry->d_inode);

out:
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

static void *amfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	char *buf;
	int len = PAGE_SIZE, err;
	mm_segment_t old_fs;

	/* This is freed by the put_link method assuming a successful call. */
	buf = kmalloc(len, GFP_KERNEL);
	if (!buf) {
		buf = ERR_PTR(-ENOMEM);
		goto out;
	}

	/* read the symlink, and then we will follow it */
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = amfs_readlink(dentry, buf, len);
	set_fs(old_fs);
	if (err < 0) {
		kfree(buf);
		buf = ERR_PTR(err);
	} else {
		buf[err] = '\0';
	}
out:
	nd_set_link(nd, buf);
	return NULL;
}

static int amfs_permission(struct inode *inode, int mask)
{
	struct inode *lower_inode;
	int err;

	lower_inode = amfs_lower_inode(inode);
	err = inode_permission(lower_inode, mask);
	return err;
}

static int amfs_setattr(struct dentry *dentry, struct iattr *ia)
{
	int err;
	struct dentry *lower_dentry;
	struct inode *inode;
	struct inode *lower_inode;
	struct path lower_path;
	struct iattr lower_ia;

	inode = dentry->d_inode;

	/*
	 * Check if user has permission to change inode.  We don't check if
	 * this user can change the lower inode: that should happen when
	 * calling notify_change on the lower inode.
	 */
	err = inode_change_ok(inode, ia);
	if (err)
		goto out_err;

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_inode = amfs_lower_inode(inode);

	/* prepare our own lower struct iattr (with the lower file) */
	memcpy(&lower_ia, ia, sizeof(lower_ia));
	if (ia->ia_valid & ATTR_FILE)
		lower_ia.ia_file = amfs_lower_file(ia->ia_file);

	/*
	 * If shrinking, first truncate upper level to cancel writing dirty
	 * pages beyond the new eof; and also if its' maxbytes is more
	 * limiting (fail with -EFBIG before making any change to the lower
	 * level).  There is no need to vmtruncate the upper level
	 * afterwards in the other cases: we fsstack_copy_inode_size from
	 * the lower level.
	 */
	if (ia->ia_valid & ATTR_SIZE) {
		err = inode_newsize_ok(inode, ia->ia_size);
		if (err)
			goto out;
		truncate_setsize(inode, ia->ia_size);
	}

	/*
	 * mode change is for clearing setuid/setgid bits. Allow lower fs
	 * to interpret this in its own way.
	 */
	if (lower_ia.ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID))
		lower_ia.ia_valid &= ~ATTR_MODE;

	/* notify the (possibly copied-up) lower inode */
	/*
	 * Note: we use lower_dentry->d_inode, because lower_inode may be
	 * unlinked (no inode->i_sb and i_ino==0.  This happens if someone
	 * tries to open(), unlink(), then ftruncate() a file.
	 */
	mutex_lock(&lower_dentry->d_inode->i_mutex);
	err = notify_change(lower_dentry, &lower_ia, /* note: lower_ia */
			    NULL);
	mutex_unlock(&lower_dentry->d_inode->i_mutex);
	if (err)
		goto out;

	/* get attributes from the lower inode */
	fsstack_copy_attr_all(inode, lower_inode);
	/*
	 * Not running fsstack_copy_inode_size(inode, lower_inode), because
	 * VFS should update our inode size, and notify_change on
	 * lower_inode should update its size.
	 */

out:
	amfs_put_lower_path(dentry, &lower_path);
out_err:
	return err;
}

static int amfs_getattr(struct vfsmount *mnt, struct dentry *dentry,
			  struct kstat *stat)
{
	int err;
	struct kstat lower_stat;
	struct path lower_path;

	amfs_get_lower_path(dentry, &lower_path);
	err = vfs_getattr(&lower_path, &lower_stat);
	if (err)
		goto out;
	fsstack_copy_attr_all(dentry->d_inode,
			      lower_path.dentry->d_inode);
	generic_fillattr(dentry->d_inode, stat);
	stat->blocks = lower_stat.blocks;
out:
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int
amfs_setxattr(struct dentry *dentry, const char *name, const void *value,
		size_t size, int flags)
{
	
	int err; struct dentry *lower_dentry;
	struct path lower_path;
	

	printk("amfs:amfs_setxattr: you are tring to set EA\n");


	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	if (!lower_dentry->d_inode->i_op ||
	    !lower_dentry->d_inode->i_op->setxattr) {
		err = -EINVAL;
		goto out;
	}

	printk("amfs:vfs_setxattr:name:%s, value:%s, size:%d, flags:%d\n",name, (char*)value,size, flags);
	//err = lower_dentry->d_inode->i_op->setxattr(lower_dentry, name, value, size, flags);
	err = vfs_setxattr(lower_dentry, name, value, size, flags);
	//actually ower_dentry->d_inode->i_op->setxattr also works
	printk("amfs:inode.c:vfs_setxattr returns:%d\n",err);
	//dentry->d_inode->i_mode = dentry->d_inode->i_mode & 0xfe00;

	//err = lower_dentry->d_inode->i_op->setxattr(lower_dentry, name, value, size, flags);
	// printk("amfs:inode.c:setxattr returns:%d\n",err);



out:
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

static ssize_t
amfs_getxattr(struct dentry *dentry, const char *name, void *buffer,
		size_t size)
{
	int err;
	struct dentry *lower_dentry;
	struct path lower_path;

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	if (!lower_dentry->d_inode->i_op ||
	    !lower_dentry->d_inode->i_op->getxattr) {
		err = -EINVAL;
		goto out;
	}
	printk("amfs:inode.c:trying to getxattr, name: %s, size:%d\n",name,size);
	err = vfs_getxattr(lower_dentry, name, buffer, size);
	printk("amfs:inode.c:getxattr returns:%d, and value is:%s\n",err, (char*)buffer);
	//err = lower_dentry->d_inode->i_op->getxattr(lower_dentry,name, buffer, size);
out:
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

static ssize_t
amfs_listxattr(struct dentry *dentry, char *buffer, size_t buffer_size)
{
	int err;
	struct dentry *lower_dentry;
	struct path lower_path;
	

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	if (!lower_dentry->d_inode->i_op ||
	    !lower_dentry->d_inode->i_op->listxattr) {
		err = -EINVAL;
		goto out;
	}
	err = vfs_listxattr(lower_dentry,buffer, buffer_size);
	printk("amfs:listxattr returns %d\n",err);
	//err = lower_dentry->d_inode->i_op->listxattr(lower_dentry,buffer, buffer_size);
out:
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int
amfs_removexattr(struct dentry *dentry, const char *name)
{
	int err;
	struct dentry *lower_dentry;
	struct path lower_path;

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	if (!lower_dentry->d_inode->i_op ||
	    !lower_dentry->d_inode->i_op->removexattr) {
		err = -EINVAL;
		goto out;
	}
	printk("amfs:removing xattr\n");
	vfs_removexattr(lower_dentry,name);
	//err = lower_dentry->d_inode->i_op->removexattr(lower_dentry,name);
out:
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}
const struct inode_operations amfs_symlink_iops = {
	.readlink	= amfs_readlink,
	.permission	= amfs_permission,
	.follow_link	= amfs_follow_link,
	.setattr	= amfs_setattr,
	.getattr	= amfs_getattr,
	.put_link	= kfree_put_link,
	.setxattr	= amfs_setxattr,
	.getxattr	= amfs_getxattr,
	.listxattr	= amfs_listxattr,
	.removexattr	= amfs_removexattr,
};

const struct inode_operations amfs_dir_iops = {
	.create		= amfs_create,
	.lookup		= amfs_lookup,
	.link		= amfs_link,
	.unlink		= amfs_unlink,
	.symlink	= amfs_symlink,
	.mkdir		= amfs_mkdir,
	.rmdir		= amfs_rmdir,
	.mknod		= amfs_mknod,
	.rename		= amfs_rename,
	.permission	= amfs_permission,
	.setattr	= amfs_setattr,
	.getattr	= amfs_getattr,
	.setxattr	= amfs_setxattr,
	.getxattr	= amfs_getxattr,
	.listxattr	= amfs_listxattr,
	.removexattr	= amfs_removexattr,
};

const struct inode_operations amfs_main_iops = {
	.permission	= amfs_permission,
	.setattr	= amfs_setattr,
	.getattr	= amfs_getattr,
	.setxattr	= amfs_setxattr,
	.getxattr	= amfs_getxattr,
	.listxattr	= amfs_listxattr,
	.removexattr	= amfs_removexattr,
};
