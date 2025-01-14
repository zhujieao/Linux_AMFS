/*
 * Copyright (c) 1998-2014 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2014 Stony Brook University
 * Copyright (c) 2003-2014 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "amfs.h"

/* The dentry cache is just so we have properly sized dentries */
static struct kmem_cache *amfs_dentry_cachep;

int amfs_init_dentry_cache(void)
{
	amfs_dentry_cachep =
		kmem_cache_create("amfs_dentry",
				  sizeof(struct amfs_dentry_info),
				  0, SLAB_RECLAIM_ACCOUNT, NULL);

	return amfs_dentry_cachep ? 0 : -ENOMEM;
}

void amfs_destroy_dentry_cache(void)
{
	if (amfs_dentry_cachep)
		kmem_cache_destroy(amfs_dentry_cachep);
}

void free_dentry_private_data(struct dentry *dentry)
{
	if (!dentry || !dentry->d_fsdata)
		return;
	kmem_cache_free(amfs_dentry_cachep, dentry->d_fsdata);
	dentry->d_fsdata = NULL;
}

/* allocate new dentry private data */
int new_dentry_private_data(struct dentry *dentry)
{
	struct amfs_dentry_info *info = AMFS_D(dentry);

	/* use zalloc to init dentry_info.lower_path */
	info = kmem_cache_zalloc(amfs_dentry_cachep, GFP_ATOMIC);
	if (!info)
		return -ENOMEM;

	spin_lock_init(&info->lock);
	dentry->d_fsdata = info;

	return 0;
}

static int amfs_inode_test(struct inode *inode, void *candidate_lower_inode)
{
	struct inode *current_lower_inode = amfs_lower_inode(inode);
	if (current_lower_inode == (struct inode *)candidate_lower_inode)
		return 1; /* found a match */
	else
		return 0; /* no match */
}

static int amfs_inode_set(struct inode *inode, void *lower_inode)
{
	/* we do actual inode initialization in amfs_iget */
	return 0;
}

struct inode *amfs_iget(struct super_block *sb, struct inode *lower_inode)
{
	struct amfs_inode_info *info;
	struct inode *inode; /* the new inode to return */
	int err;

	inode = iget5_locked(sb, /* our superblock */
			     /*
			      * hashval: we use inode number, but we can
			      * also use "(unsigned long)lower_inode"
			      * instead.
			      */
			     lower_inode->i_ino, /* hashval */
			     amfs_inode_test,	/* inode comparison function */
			     amfs_inode_set, /* inode init function */
			     lower_inode); /* data passed to test+set fxns */
	if (!inode) {
		err = -EACCES;
		iput(lower_inode);
		return ERR_PTR(err);
	}
	/* if found a cached inode, then just return it */
	if (!(inode->i_state & I_NEW))
		return inode;

	/* initialize new inode */
	info = AMFS_I(inode);

	inode->i_ino = lower_inode->i_ino;
	if (!igrab(lower_inode)) {
		err = -ESTALE;
		return ERR_PTR(err);
	}
	amfs_set_lower_inode(inode, lower_inode);

	inode->i_version++;

	/* use different set of inode ops for symlinks & directories */
	if (S_ISDIR(lower_inode->i_mode))
		inode->i_op = &amfs_dir_iops;
	else if (S_ISLNK(lower_inode->i_mode))
		inode->i_op = &amfs_symlink_iops;
	else
		inode->i_op = &amfs_main_iops;

	/* use different set of file ops for directories */
	if (S_ISDIR(lower_inode->i_mode))
		inode->i_fop = &amfs_dir_fops;
	else
		inode->i_fop = &amfs_main_fops;

	inode->i_mapping->a_ops = &amfs_aops;

	inode->i_atime.tv_sec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_mtime.tv_sec = 0;
	inode->i_mtime.tv_nsec = 0;
	inode->i_ctime.tv_sec = 0;
	inode->i_ctime.tv_nsec = 0;

	/* properly initialize special inodes */
	if (S_ISBLK(lower_inode->i_mode) || S_ISCHR(lower_inode->i_mode) ||
	    S_ISFIFO(lower_inode->i_mode) || S_ISSOCK(lower_inode->i_mode))
		init_special_inode(inode, lower_inode->i_mode,
				   lower_inode->i_rdev);

	/* all well, copy inode attributes */
	fsstack_copy_attr_all(inode, lower_inode);
	fsstack_copy_inode_size(inode, lower_inode);

	unlock_new_inode(inode);
	return inode;
}

/*
 * Connect a amfs inode dentry/inode with several lower ones.  This is
 * the classic stackable file system "vnode interposition" action.
 *
 * @dentry: amfs's dentry which interposes on lower one
 * @sb: amfs's super_block
 * @lower_path: the lower path (caller does path_get/put)
 */
int amfs_interpose(struct dentry *dentry, struct super_block *sb,
		     struct path *lower_path)
{
	int err = 0;
	struct inode *inode;
	struct inode *lower_inode;
	struct super_block *lower_sb;

	lower_inode = lower_path->dentry->d_inode;
	lower_sb = amfs_lower_super(sb);

	/* check that the lower file system didn't cross a mount point */
	if (lower_inode->i_sb != lower_sb) {
		err = -EXDEV;
		goto out;
	}

	/*
	 * We allocate our new inode below by calling amfs_iget,
	 * which will initialize some of the new inode's fields
	 */

	/* inherit lower inode number for amfs's inode */
	inode = amfs_iget(sb, lower_inode);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out;
	}

	d_add(dentry, inode);

out:
	return err;
}

/*
 * Main driver function for amfs's lookup.
 *
 * Returns: NULL (ok), ERR_PTR if an error occurred.
 * Fills in lower_parent_path with <dentry,mnt> on success.
 */
static struct dentry *__amfs_lookup(struct dentry *dentry,
				      unsigned int flags,
				      struct path *lower_parent_path)
{
	int err = 0;
	struct vfsmount *lower_dir_mnt;
	struct dentry *lower_dir_dentry = NULL;
	struct dentry *lower_dentry;
	const char *name;
	struct path lower_path;
	struct qstr this;

	/* must initialize dentry operations */
	d_set_d_op(dentry, &amfs_dops);

	if (IS_ROOT(dentry))
		goto out;

	name = dentry->d_name.name;

	/* now start the actual lookup procedure */
	lower_dir_dentry = lower_parent_path->dentry;
	lower_dir_mnt = lower_parent_path->mnt;

	/* Use vfs_path_lookup to check if the dentry exists or not */
	err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, name, 0,
			      &lower_path);

	/* no error: handle positive dentries */
	if (!err) {
		amfs_set_lower_path(dentry, &lower_path);
		err = amfs_interpose(dentry, dentry->d_sb, &lower_path);
		if (err) /* path_put underlying path on error */
			amfs_put_reset_lower_path(dentry);
		goto out;
	}

	/*
	 * We don't consider ENOENT an error, and we want to return a
	 * negative dentry.
	 */
	if (err && err != -ENOENT)
		goto out;

	/* instatiate a new negative dentry */
	this.name = name;
	this.len = strlen(name);
	this.hash = full_name_hash(this.name, this.len);
	lower_dentry = d_lookup(lower_dir_dentry, &this);
	if (lower_dentry)
		goto setup_lower;

	lower_dentry = d_alloc(lower_dir_dentry, &this);
	if (!lower_dentry) {
		err = -ENOMEM;
		goto out;
	}
	d_add(lower_dentry, NULL); /* instantiate and hash */

setup_lower:
	lower_path.dentry = lower_dentry;
	lower_path.mnt = mntget(lower_dir_mnt);
	amfs_set_lower_path(dentry, &lower_path);

	/*
	 * If the intent is to create a file, then don't return an error, so
	 * the VFS will continue the process of making this negative dentry
	 * into a positive one.
	 */
	if (flags & (LOOKUP_CREATE|LOOKUP_RENAME_TARGET))
		err = 0;

out:
	//err = 0;
	return ERR_PTR(err);
}

struct dentry *amfs_lookup(struct inode *dir, struct dentry *dentry,
			     unsigned int flags)
{
	int err;
	struct dentry *ret, *parent;
	struct path lower_parent_path;


	char *xattr_value = (char*)kmalloc(3,GFP_KERNEL);



	printk("amfs:lookup.c->amfs_lookup\n");

	parent = dget_parent(dentry);

	amfs_get_lower_path(parent, &lower_parent_path);

	/* allocate dentry private data.  We free it in ->d_release */
	err = new_dentry_private_data(dentry);
	if (err) {
		ret = ERR_PTR(err);
		goto out;
	}
	ret = __amfs_lookup(dentry, flags, &lower_parent_path);
	if (IS_ERR(ret))
		goto out;
	if (ret) 
		dentry = ret;
	if (dentry->d_inode)
		fsstack_copy_attr_times(dentry->d_inode,
					amfs_lower_inode(dentry->d_inode));
	/* update parent directory's atime */
	fsstack_copy_attr_atime(parent->d_inode,
				amfs_lower_inode(parent->d_inode));

	if(ret == NULL){
		printk("amfs:lookup.c:ret == NULL\n");
	}else{
		if(vfs_getxattr(ret, (const char *)&"security.bad", (void *)xattr_value, 3)>0 && memcmp(xattr_value,&"yes",3)==0){
			printk("amfs:lookup.c:bad file found\n");
			ret = ERR_PTR(-EACCES);
			goto out;
		}
	}

out:
	kfree(xattr_value);
	amfs_put_lower_path(parent, &lower_parent_path);
	dput(parent);
	return ret;
}
