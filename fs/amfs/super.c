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

/*
 * The inode cache is used with alloc_inode for both our inode info and the
 * vfs inode.
 */
static struct kmem_cache *amfs_inode_cachep;

/* final actions when unmounting a file system */
static void amfs_put_super(struct super_block *sb)
{
	struct amfs_sb_info *spd;
	struct super_block *s;
	struct list_head *node;
	struct pattern_node *a_node, *tmp_node;
	struct file *pattern_file=NULL;
	loff_t pos = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	spd = AMFS_SB(sb);
	if (!spd)
		return;

	/* decrement lower super references */
	s = amfs_lower_super(sb);
	amfs_set_lower_super(sb, NULL);
	atomic_dec(&s->s_active);
	printk("amfs:super.c:WRAPFS_SUPER_MAGIC: %u, %08x\n",WRAPFS_SUPER_MAGIC, WRAPFS_SUPER_MAGIC);

	//pattern_file = filp_open(AMFS_SB(sb)->pattern_db_path+7, O_RDONLY, 0644);
	//mutex_unlock(&pattern_file->f_inode->i_mutex);
	//filp_close(pattern_file,0);

	AMFS_SB(sb)->pattern_inode->i_size = AMFS_SB(sb)->i_size;
 
	printk("amfs: umount: openning %s\n",AMFS_SB(sb)->pattern_db_path+7);
	pattern_file = filp_open(AMFS_SB(sb)->pattern_db_path, O_RDWR|O_CREAT|O_TRUNC, 0644); 
	if (IS_ERR(pattern_file)) {
		printk(KERN_ERR
		       "amfs: umount: unable to open pattern file %s, error no: %d\n", AMFS_SB(sb)->pattern_db_path,(int)pattern_file);
	}else{
		list_for_each(node, &AMFS_SB(sb)->pattern_head->list){
			tmp_node = list_entry(node, struct pattern_node, list);
			vfs_write(pattern_file, tmp_node->data, tmp_node->length, &pos);
			vfs_write(pattern_file, (const char *)&"\n", 1, &pos);
		}
		pattern_file->f_inode->i_mode = AMFS_SB(sb)->i_mode;
		filp_close(pattern_file,0);
	}
	kfree(AMFS_SB(sb)->pattern_db_path);



	list_for_each_entry_safe(a_node, tmp_node, &AMFS_SB(sb)->pattern_head->list, list) {
	    printk("amfs:umount:free:length:%d\n",a_node->length);
	    printk("amfs:umount:free:data:%s\n",a_node->data);
	    kfree(a_node->data);
	    list_del(&a_node->list);
	    kfree(a_node);
	}

	
	// printk("amfs:umount:free:length:%d\n",AMFS_SB(sb)->pattern_head->length);
	// printk("amfs:umount:free:data:%s\n",AMFS_SB(sb)->pattern_head->data);
	// kfree(AMFS_SB(sb)->pattern_head->data);
	kfree(AMFS_SB(sb)->pattern_head);




	// list_for_each(node, &AMFS_SB(sb)->pattern_head->list){
	// 	tmp_node = list_entry(node, struct pattern_node, list);
	// 	kfree(tmp_node->data);
	// }
	// kfree(AMFS_SB(sb)->pattern_head->data);

	//kfree(spd->pattern_data);
	kfree(spd);
	set_fs(old_fs);
	sb->s_fs_info = NULL;
}

static int amfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	int err;
	struct path lower_path;

	amfs_get_lower_path(dentry, &lower_path);
	err = vfs_statfs(&lower_path, buf);
	amfs_put_lower_path(dentry, &lower_path);

	/* set return buf to our f/s to avoid confusing user-level utils */
	buf->f_type = WRAPFS_SUPER_MAGIC;
	printk("amfs:super.c:WRAPFS_SUPER_MAGIC: %du",WRAPFS_SUPER_MAGIC);

	return err;
}

/*
 * @flags: numeric mount options
 * @options: mount options string
 */
static int amfs_remount_fs(struct super_block *sb, int *flags, char *options)
{
	int err = 0;

	/*
	 * The VFS will take care of "ro" and "rw" flags among others.  We
	 * can safely accept a few flags (RDONLY, MANDLOCK), and honor
	 * SILENT, but anything else left over is an error.
	 */
	if ((*flags & ~(MS_RDONLY | MS_MANDLOCK | MS_SILENT)) != 0) {
		printk(KERN_ERR
		       "amfs: remount flags 0x%x unsupported\n", *flags);
		err = -EINVAL;
	}

	return err;
}

/*
 * Called by iput() when the inode reference count reached zero
 * and the inode is not hashed anywhere.  Used to clear anything
 * that needs to be, before the inode is completely destroyed and put
 * on the inode free list.
 */
static void amfs_evict_inode(struct inode *inode)
{
	struct inode *lower_inode;

	truncate_inode_pages(&inode->i_data, 0);
	clear_inode(inode);
	/*
	 * Decrement a reference to a lower_inode, which was incremented
	 * by our read_inode when it was created initially.
	 */
	lower_inode = amfs_lower_inode(inode);
	amfs_set_lower_inode(inode, NULL);
	iput(lower_inode);
}

static struct inode *amfs_alloc_inode(struct super_block *sb)
{
	struct amfs_inode_info *i;

	i = kmem_cache_alloc(amfs_inode_cachep, GFP_KERNEL);
	if (!i)
		return NULL;

	/* memset everything up to the inode to 0 */
	memset(i, 0, offsetof(struct amfs_inode_info, vfs_inode));

	i->vfs_inode.i_version = 1;
	return &i->vfs_inode;
}

static void amfs_destroy_inode(struct inode *inode)
{
	kmem_cache_free(amfs_inode_cachep, AMFS_I(inode));
}

/* amfs inode cache constructor */
static void init_once(void *obj)
{
	struct amfs_inode_info *i = obj;

	inode_init_once(&i->vfs_inode);
}

int amfs_init_inode_cache(void)
{
	int err = 0;

	amfs_inode_cachep =
		kmem_cache_create("amfs_inode_cache",
				  sizeof(struct amfs_inode_info), 0,
				  SLAB_RECLAIM_ACCOUNT, init_once);
	if (!amfs_inode_cachep)
		err = -ENOMEM;
	return err;
}

/* amfs inode cache destructor */
void amfs_destroy_inode_cache(void)
{
	if (amfs_inode_cachep)
		kmem_cache_destroy(amfs_inode_cachep);
}

/*
 * Used only in nfs, to kill any pending RPC tasks, so that subsequent
 * code can actually succeed and won't leave tasks that need handling.
 */
static void amfs_umount_begin(struct super_block *sb)
{
	struct super_block *lower_sb;

	lower_sb = amfs_lower_super(sb);
	if (lower_sb && lower_sb->s_op && lower_sb->s_op->umount_begin)
		lower_sb->s_op->umount_begin(lower_sb);
}

const struct super_operations amfs_sops = {
	.put_super	= amfs_put_super,
	.statfs		= amfs_statfs,
	.remount_fs	= amfs_remount_fs,
	.evict_inode	= amfs_evict_inode,
	.umount_begin	= amfs_umount_begin,
	.show_options	= generic_show_options,
	.alloc_inode	= amfs_alloc_inode,
	.destroy_inode	= amfs_destroy_inode,
	.drop_inode	= generic_delete_inode,
};
