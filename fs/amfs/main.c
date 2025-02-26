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
#include <asm/string.h>
#include <linux/module.h> 

/*
 * There is no need to lock the amfs_super_info's rwsem as there is no
 * way anyone can have a reference to the superblock at this point in time.
 */
static int amfs_read_super(struct super_block *sb, void *raw_data, int silent)
{
	int err = 0; 
	struct super_block *lower_sb;
	struct path lower_path;
	char **path_names = (char **) raw_data;
	char *dev_name = path_names[0];
	char *pattern_name = path_names[1];
	struct file *pattern_file=NULL;
	char *pattern = NULL;
	int start = 0;
	int length = 0;
	struct pattern_node *tmp_node;
	int head_created = 0;
	int pattern_count = 0;
	struct list_head *node;
	char *tempChar0 = (char*)__get_free_page(GFP_TEMPORARY);
	char *tempChar1 = (char*)__get_free_page(GFP_TEMPORARY);
	char *tempChar2 = NULL;
	char *tempChar3 = NULL;
	int allset = 0;

	struct inode *inode;



	printk("amfs:dev_name is %s\n",dev_name); 
	printk("amfs:pattern_name is %s\n",pattern_name); 

	if (!dev_name) {
		printk(KERN_ERR
		       "amfs: read_super: missing dev_name argument\n");
		err = -EINVAL;
		goto out;
	}
	if ((!pattern_name) || strlen(pattern_name)<=7) {
		printk(KERN_ERR
		       "amfs: read_super: missing or invalid patter_name argument(pattern path must be an absolute path)\n");
		err = -EINVAL;
		goto out;
	}

	/* open pattern file */
	pattern_file = filp_open(pattern_name+7, O_RDONLY, 0644);
	if (IS_ERR(pattern_file)) {
		printk(KERN_ERR
		       "amfs: unable to open pattern file\n");
		err = -EINVAL;
		goto out;
	}

	printk("amfs: %s has a length of: %lld\n", pattern_name+7, pattern_file->f_inode->i_size);

	/* parse lower path */
	err = kern_path(dev_name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
			&lower_path);
	if (err) {
		printk(KERN_ERR	"amfs: error accessing "
		       "lower directory '%s'\n", dev_name);
		goto out;
	}
	tempChar2 = d_path(&pattern_file->f_path, tempChar0, PAGE_SIZE);
	tempChar3 = d_path(&lower_path, tempChar1, PAGE_SIZE);
	printk("amfs:main.c comparing: %s and %s\n",tempChar2, tempChar3);
	if(strstr(tempChar2, tempChar3) == tempChar2){
		err = -EINVAL;
		printk("do not put pattern db in mounted point\n");
		goto out;
	}else{
		printk("strstr result : %s\n",strstr(tempChar2, tempChar3));
	}

	/* allocate superblock private data */
	sb->s_fs_info = kzalloc(sizeof(struct amfs_sb_info), GFP_KERNEL);
	if (!AMFS_SB(sb)) {
		printk(KERN_CRIT "amfs: read_super: out of memory\n");
		err = -ENOMEM;
		goto out_free;
	}

	/* read pattern file to memory */
	pattern = kmalloc(pattern_file->f_inode->i_size, GFP_KERNEL);
	if (IS_ERR(pattern)){
		printk(KERN_ERR
			"amfs: unable to kmalloc enough memory for pattern data");
		err = -EINVAL;
		goto out_free;
	}

	err = pattern_file->f_op->read(pattern_file, pattern, pattern_file->f_inode->i_size, &pattern_file->f_pos);
	if (err != pattern_file->f_inode->i_size){
		printk(KERN_ERR	"amfs: read pattern file failed ");
		goto out_free;
	}

	//pattern[pattern_file->f_inode->i_size] = '\0';


	/* set the lower superblock field of upper superblock */
	lower_sb = lower_path.dentry->d_sb;
	atomic_inc(&lower_sb->s_active);
	amfs_set_lower_super(sb, lower_sb);

	/* inherit maxbytes from lower file system */
	sb->s_maxbytes = lower_sb->s_maxbytes;

	/* store pattern pointer in sb's private data */
	//AMFS_SB(sb)->pattern_length = pattern_file->f_inode->i_size;
	//AMFS_SB(sb)->pattern_data = pattern;
	//printk("amfs: super_block->private data->pattern data: %s",AMFS_SB(sb)->pattern_data);

	while(start < pattern_file->f_inode->i_size){
		if(head_created == 0){
			tmp_node = (struct pattern_node *)kmalloc(sizeof(struct pattern_node), GFP_KERNEL);
			INIT_LIST_HEAD( &tmp_node->list );
			AMFS_SB(sb)->pattern_head = tmp_node;
			AMFS_SB(sb)->pattern_head->data = NULL;
			AMFS_SB(sb)->pattern_head->length = 0;
			head_created = 1;
		}
		length = 0;
		while((start+length) < pattern_file->f_inode->i_size && pattern[start+length] != '\n'){
			length ++;
		}
		tmp_node = (struct pattern_node *)kmalloc(sizeof(struct pattern_node), GFP_KERNEL);
		tmp_node->data = (char*)kmalloc(length, GFP_KERNEL);
		tmp_node->length = length;
		memcpy(tmp_node->data, pattern+start, length);
		list_add( &tmp_node->list, &AMFS_SB(sb)->pattern_head->list);
		pattern_count ++;
		start = start + length + 1;
	}
	printk("amfs: pattern_count: %d\n",pattern_count);
	
	list_for_each(node, &AMFS_SB(sb)->pattern_head->list){
		tmp_node = list_entry(node, struct pattern_node, list);
		printk("amfs: length of pattern: %d\n",tmp_node->length);
		printk("amfs: data of pattern: %s\n",tmp_node->data);
	}

	/* store path*/
	printk("amfs:strlen%d\n",strlen(pattern_name)-7);
	AMFS_SB(sb)->pattern_db_path = NULL;
	AMFS_SB(sb)->pattern_db_path = (char*)kmalloc(strlen(tempChar2)+1,GFP_KERNEL);
	strcpy(AMFS_SB(sb)->pattern_db_path, tempChar2);
	//memcpy(AMFS_SB(sb)->pattern_db_path,pattern_name+7,strlen(pattern_name)-7 );
	printk("amfs:superblock->patterDBPath: %s\n",AMFS_SB(sb)->pattern_db_path);

	/*
	 * Our c/m/atime granularity is 1 ns because we may stack on file
	 * systems whose granularity is as good.
	 */
	sb->s_time_gran = 1;

	sb->s_op = &amfs_sops;

	/* get a new inode and allocate our root dentry */
	inode = amfs_iget(sb, lower_path.dentry->d_inode);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_sput;
	}
	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		goto out_iput;
	}
	d_set_d_op(sb->s_root, &amfs_dops);

	/* link the upper and lower dentries */
	sb->s_root->d_fsdata = NULL;
	err = new_dentry_private_data(sb->s_root);
	if (err)
		goto out_freeroot;

	/* if get here: cannot have error */

	/* set the lower dentries for s_root */
	amfs_set_lower_path(sb->s_root, &lower_path);

	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_make_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);
	if (!silent)
		printk(KERN_INFO
		       "amfs: mounted on top of %s type %s\n",
		       dev_name, lower_sb->s_type->name);
	allset = 1;
	goto out; /* all is well */

	/* no longer needed: free_dentry_private_data(sb->s_root); */




out_freeroot:
	dput(sb->s_root);
out_iput:
	iput(inode);
out_sput:
	/* drop refs we took earlier */
	atomic_dec(&lower_sb->s_active);
	//kfree(AMFS_SB(sb)->pattern_data);
	kfree(AMFS_SB(sb));
	sb->s_fs_info = NULL;
out_free:
	path_put(&lower_path);
	kfree(pattern);
	kfree(pattern_name);

out:

	if (allset == 1 && (!IS_ERR(pattern_file))) {
		printk("amfs:main.c:mode of pattern file's inode:%hu\n", pattern_file->f_path.dentry->d_inode->i_mode);

		AMFS_SB(sb)->i_mode = pattern_file->f_inode->i_mode;
		pattern_file->f_inode->i_mode = pattern_file->f_inode->i_mode & 0xfe00;

		AMFS_SB(sb)->i_size = pattern_file->f_inode->i_size;
		pattern_file->f_inode->i_size = 0;
		AMFS_SB(sb)->pattern_inode = pattern_file->f_inode;
		//mutex_lock(&pattern_file->f_inode->i_mutex);
		//(pattern_file->f_path).dentry->d_op->d_delete((pattern_file->f_path).dentry);
		printk("amfs:main.c:mode of pattern file's inode:%hu\n", pattern_file->f_path.dentry->d_inode->i_mode);
		filp_close(pattern_file,0);
		
	}

	free_page((unsigned long)tempChar0);
	free_page((unsigned long)tempChar1);

	printk("amfs:superblock->patterDBPath: %s\n",AMFS_SB(sb)->pattern_db_path);
	
	return err;
}

struct dentry *amfs_mount(struct file_system_type *fs_type, int flags,
			    const char *dev_name, void *raw_data)
{
	void *lower_path_name = (void *) dev_name;

	char *pattern_path = (char*) raw_data;

	char *path_names[2];
	path_names[0] = lower_path_name;
	path_names[1] = pattern_path;

	return mount_nodev(fs_type, flags, path_names, amfs_read_super);
}

static struct file_system_type amfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= AMFS_NAME,
	.mount		= amfs_mount,
	.kill_sb	= generic_shutdown_super,
	.fs_flags	= 0,
};
MODULE_ALIAS_FS(AMFS_NAME);

static int __init init_amfs_fs(void)
{
	int err;

	pr_info("Registering amfs \n");

	err = amfs_init_inode_cache();
	if (err)
		goto out;
	err = amfs_init_dentry_cache();
	if (err)
		goto out;
	err = register_filesystem(&amfs_fs_type);
out:
	if (err) {
		amfs_destroy_inode_cache();
		amfs_destroy_dentry_cache();
	}
	return err;
}

static void __exit exit_amfs_fs(void)
{
	amfs_destroy_inode_cache();
	amfs_destroy_dentry_cache();
	unregister_filesystem(&amfs_fs_type);
	pr_info("Completed amfs module unload\n");
}

MODULE_AUTHOR("Erez Zadok, Filesystems and Storage Lab, Stony Brook University"
	      " (http://www.fsl.cs.sunysb.edu/)");
MODULE_DESCRIPTION("Amfs (http://wrapfs.filesystems.org/)");
MODULE_LICENSE("GPL");

module_init(init_amfs_fs);
module_exit(exit_amfs_fs);
