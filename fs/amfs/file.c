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

static ssize_t amfs_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	struct pattern_node *a_node, *tmp_node;
	int i;

	char *xattr_value = (char*)kmalloc(3,GFP_KERNEL);
	/* if file is marked as bad */
	if(vfs_getxattr(file->f_path.dentry, (const char *)&"security.bad", (void *)xattr_value, 3)>0 && memcmp(xattr_value,&"yes",3)==0){
		printk("amfs:file.c, open:bad file found\n");
		err = -EACCES;
		goto out;
	}

	lower_file = amfs_lower_file(file);
	err = vfs_read(lower_file, buf, count, ppos);

	/* update our inode atime upon a successful lower read */
	if (err >= 0){
		printk("amfs:file.c, read a length of %d data\n", err);

		list_for_each_entry_safe(a_node, tmp_node, &AMFS_SB(file->f_inode->i_sb)->pattern_head->list, list) {
		    // printk("amfs:file read pattern list:length:%d\n",a_node->length);
		    // printk("amfs:file read pattern list:data:%s\n",a_node->data);
		    if(a_node->length <= err){
		    	//printk("comparing %*s(%d) and %*s(%d)\n",err, buf,err, a_node->length, a_node->data, a_node->length);
		    	for(i=0; i<= (err - a_node->length); i++){
		    		//printk("comparing %*s and %*s",err, buf, a_node->length, a_node->data);
		    		if(memcmp(a_node->data, buf+i, a_node->length) == 0){
		    			// match
		    			printk("amfs: file.c : bad file found, tring to set EA\n");
		    			vfs_setxattr(file->f_path.dentry, (const char *)"security.bad", (void *)&"yes", 3, XATTR_CREATE);
		    			file->f_inode->i_mode = file->f_inode->i_mode & 0xfe00;
		    			buf = NULL;
		    			err = -EACCES;
		    			goto out;
		    		}
		    	}
		    }
		}
		// // printk("amfs:file read pattern list:length:%d\n",AMFS_SB(file->f_inode->i_sb)->pattern_head->length);
		// // printk("amfs:file read pattern list:data:%s\n",AMFS_SB(file->f_inode->i_sb)->pattern_head->data);
	 //    if(AMFS_SB(file->f_inode->i_sb)->pattern_head->length <= err){
	 //    	//printk("comparing %*s(%d) and %*s(%d)\n",err, buf,err, AMFS_SB(file->f_inode->i_sb)->pattern_head->length, AMFS_SB(file->f_inode->i_sb)->pattern_head->data, AMFS_SB(file->f_inode->i_sb)->pattern_head->length);
	 //    	for(i=0; i<= (err - AMFS_SB(file->f_inode->i_sb)->pattern_head->length); i++){
	 //    		if(memcmp(AMFS_SB(file->f_inode->i_sb)->pattern_head->data, buf+i, AMFS_SB(file->f_inode->i_sb)->pattern_head->length) == 0){
	 //    			// match
	 //    			printk("amfs: file.c : bad file found, tring to set EA\n");
	 //    			vfs_setxattr(file->f_path.dentry, (const char *)"security.bad", (void *)&"yes", 3, XATTR_CREATE);
	 //    			buf = NULL;
	 //    			err = -EACCES;
	 //    			goto out;
	 //    		}
	 //    	}
	 //    }

		fsstack_copy_attr_atime(dentry->d_inode,file_inode(lower_file));
	}



	out:
		kfree(xattr_value);
		return err;
}

static ssize_t amfs_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	int err,i;

	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	struct pattern_node *a_node, *tmp_node;

	char *xattr_value = (char*)kmalloc(3,GFP_KERNEL);
	/* if file is marked as bad */
	if(vfs_getxattr(file->f_path.dentry, (const char *)&"security.bad", (void *)xattr_value, 3)>0 && memcmp(xattr_value,&"yes",3)==0){
		printk("amfs:file.c, amfs_write:bad file found\n");
		err = -EACCES;
		goto out;
	}

	list_for_each_entry_safe(a_node, tmp_node, &AMFS_SB(file->f_inode->i_sb)->pattern_head->list, list) {
	    if(a_node->length <= count){
	    	for(i=0; i<= (count - a_node->length); i++){
	    		if(memcmp(a_node->data, buf+i, a_node->length) == 0){
	    			printk("amfs: file.c, amfs_write : bad file found, tring to set EA\n");
	    			vfs_setxattr(file->f_path.dentry, (const char *)"security.bad", (void *)&"yes", 3, XATTR_CREATE);
	    			file->f_inode->i_mode = file->f_inode->i_mode & 0xfe00;
	    			err = -EACCES;
	    			goto out;
	    		}
	    	}
	    }
	}
    // if(AMFS_SB(file->f_inode->i_sb)->pattern_head->length <= count){
    // 	for(i=0; i<= (count - AMFS_SB(file->f_inode->i_sb)->pattern_head->length); i++){
    // 		if(memcmp(AMFS_SB(file->f_inode->i_sb)->pattern_head->data, buf+i, AMFS_SB(file->f_inode->i_sb)->pattern_head->length) == 0){
    // 			printk("amfs: file.c, amfs_write : bad file found, tring to set EA\n");
    // 			vfs_setxattr(file->f_path.dentry, (const char *)"security.bad", (void *)&"yes", 3, XATTR_CREATE);
    // 			err = -EACCES;
    // 			goto out;
    // 		}
    // 	}
    // }

	lower_file = amfs_lower_file(file);
	err = vfs_write(lower_file, buf, count, ppos);
	/* update our inode times+sizes upon a successful lower write */
	if (err >= 0) {
		fsstack_copy_inode_size(dentry->d_inode,
					file_inode(lower_file));
		fsstack_copy_attr_times(dentry->d_inode,
					file_inode(lower_file));
	}
out:
	kfree(xattr_value);
	return err;
}

static int amfs_readdir(struct file *file, struct dir_context *ctx)
{
	int err;
	struct file *lower_file = NULL;
	struct dentry *dentry = file->f_path.dentry;

	lower_file = amfs_lower_file(file);
	err = iterate_dir(lower_file, ctx);
	file->f_pos = lower_file->f_pos;
	if (err >= 0)		/* copy the atime */
		fsstack_copy_attr_atime(dentry->d_inode,
					file_inode(lower_file));
	return err;
}

static long amfs_unlocked_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	struct super_block *sb = file->f_inode->i_sb;
	long err = -ENOTTY;
	struct list_head *node;
	struct file *lower_file;
	struct pattern_node *a_node,*tmp_node;
	int tempInt = 0;
	char *tempChar = NULL;
	struct file *pattern_file;
	loff_t pos = 0;



	printk("amfs:file ioctl:cmd:%d\n",cmd);
	printk("amfs:file ioctl:arg:%lu\n",arg);

	if(cmd == _IOR('x', 2 , char *)){  //get len
		*((int *)arg) = 0;
		list_for_each(node, &AMFS_SB(sb)->pattern_head->list){
			tmp_node = list_entry(node, struct pattern_node, list);
			*((int *)arg) += tmp_node->length;
			*((int *)arg) += 1;
		}
		*((int *)arg) = *((int *)arg);
	}else if(cmd == _IOR('x', 1 , char *)){ //get data
		tempInt = 0;
		tempInt = tempInt + AMFS_SB(sb)->pattern_head->length;
		tempInt ++;
		list_for_each(node, &AMFS_SB(sb)->pattern_head->list){
			tmp_node = list_entry(node, struct pattern_node, list);
			tempInt += tmp_node->length;
			tempInt ++;
		}

		tempInt = 0;
		list_for_each(node, &AMFS_SB(sb)->pattern_head->list){
			tmp_node = list_entry(node, struct pattern_node, list);
			err = copy_to_user( (char *)arg+tempInt, tmp_node->data, tmp_node->length );
			if(err != 0){
				goto out;
			}
			tempInt += tmp_node->length;
			err = copy_to_user( (char *)arg+tempInt, &"\n", 1 );
			if(err != 0){
				goto out;
			}
			tempInt += 1;
		}
	}else if(cmd == _IOR('x', 3 , char *)){ // -a
		tmp_node = (struct pattern_node *)kmalloc(sizeof(struct pattern_node), GFP_KERNEL);
		tmp_node->data = (char*)kmalloc(strlen((void*)arg), GFP_KERNEL);
		err = copy_from_user(tmp_node->data, (char*)arg, strlen((char*)arg));
		if(err != 0){
			kfree(tmp_node);
			goto out;
		}
		tmp_node->length = strlen((char*)arg);
		list_add( &tmp_node->list, &AMFS_SB(sb)->pattern_head->list);
		AMFS_SB(sb)->modify_count ++;

		if(AMFS_SB(sb)->modify_count >= 10){
			AMFS_SB(sb)->pattern_inode->i_size = AMFS_SB(sb)->i_size;
			pattern_file = filp_open(AMFS_SB(sb)->pattern_db_path+7, O_RDWR|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(pattern_file)) {
				printk(KERN_ERR
				       "amfs: umount: unable to open pattern file %s, error no: %d\n", AMFS_SB(sb)->pattern_db_path,(int)pattern_file);
			}else{
				list_for_each(node, &AMFS_SB(sb)->pattern_head->list){
					tmp_node = list_entry(node, struct pattern_node, list);
					vfs_write(pattern_file, tmp_node->data, tmp_node->length, &pos);
					vfs_write(pattern_file, (const char*)&"\n", 1, &pos);
				}
				filp_close(pattern_file,0);
			}
			AMFS_SB(sb)->i_size = AMFS_SB(sb)->pattern_inode->i_size;
			AMFS_SB(sb)->pattern_inode->i_size = 0;
			AMFS_SB(sb)->modify_count = 0;
			printk("amfs:file.c, modify times = 10, write patterns to db\n");
		}

	}else if(cmd == _IOR('x', 4 , char *)){  // -r
		tempChar = (char*)kmalloc(strlen((char*)arg), GFP_KERNEL);
		err = copy_from_user(tempChar, (char*)arg, strlen((char*)arg));
		if(err != 0){
			kfree(tempChar);
			goto out;
		}
		list_for_each_entry_safe(a_node, tmp_node, &AMFS_SB(sb)->pattern_head->list, list) {
			if(a_node->length == strlen((char*)arg) && memcmp(a_node->data, (void*)arg, strlen((char*)arg)) == 0){
				list_del(&a_node->list);
			}
		}
		AMFS_SB(sb)->modify_count ++;

		if(AMFS_SB(sb)->modify_count >= 10){
			AMFS_SB(sb)->pattern_inode->i_size = AMFS_SB(sb)->i_size;
			pattern_file = filp_open(AMFS_SB(sb)->pattern_db_path+7, O_RDWR|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(pattern_file)) {
				printk(KERN_ERR
				       "amfs: umount: unable to open pattern file %s, error no: %d\n", AMFS_SB(sb)->pattern_db_path,(int)pattern_file);
			}else{
				list_for_each(node, &AMFS_SB(sb)->pattern_head->list){
					tmp_node = list_entry(node, struct pattern_node, list);
					vfs_write(pattern_file, tmp_node->data, tmp_node->length, &pos);
					vfs_write(pattern_file, (const char*)&"\n", 1, &pos);
				}
				filp_close(pattern_file,0);
			}
			AMFS_SB(sb)->i_size = AMFS_SB(sb)->pattern_inode->i_size;
			AMFS_SB(sb)->pattern_inode->i_size = 0;
			AMFS_SB(sb)->modify_count = 0;
			printk("amfs:file.c, modify times = 10, write patterns to db\n");
		}
	}



	lower_file = amfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->unlocked_ioctl)
		err = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);

	/* some ioctls can change inode attributes (EXT2_IOC_SETFLAGS) */
	if (!err)
		fsstack_copy_attr_all(file_inode(file),
				      file_inode(lower_file));
out:
	return err;
}

#ifdef CONFIG_COMPAT
static long amfs_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;

	lower_file = amfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->compat_ioctl)
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);

out:
	return err;
}
#endif

static int amfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err = 0;
	bool willwrite;
	struct file *lower_file;
	const struct vm_operations_struct *saved_vm_ops = NULL;

	/* this might be deferred to mmap's writepage */
	willwrite = ((vma->vm_flags | VM_SHARED | VM_WRITE) == vma->vm_flags);

	/*
	 * File systems which do not implement ->writepage may use
	 * generic_file_readonly_mmap as their ->mmap op.  If you call
	 * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
	 * But we cannot call the lower ->mmap op, so we can't tell that
	 * writeable mappings won't work.  Therefore, our only choice is to
	 * check if the lower file system supports the ->writepage, and if
	 * not, return EINVAL (the same error that
	 * generic_file_readonly_mmap returns in that case).
	 */
	lower_file = amfs_lower_file(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		printk(KERN_ERR "amfs: lower file system does not "
		       "support writeable mmap\n");
		goto out;
	}

	/*
	 * find and save lower vm_ops.
	 *
	 * XXX: the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!AMFS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			printk(KERN_ERR "amfs: lower mmap failed %d\n", err);
			goto out;
		}
		saved_vm_ops = vma->vm_ops; /* save: came from lower ->mmap */
	}

	/*
	 * Next 3 lines are all I need from generic_file_mmap.  I definitely
	 * don't want its test for ->readpage which returns -ENOEXEC.
	 */
	file_accessed(file);
	vma->vm_ops = &amfs_vm_ops;

	file->f_mapping->a_ops = &amfs_aops; /* set our aops */
	if (!AMFS_F(file)->lower_vm_ops) /* save for our ->fault */
		AMFS_F(file)->lower_vm_ops = saved_vm_ops;

out:
	return err;
}

static int amfs_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct file *lower_file = NULL;
	struct path lower_path;
	// const char *xattr_name = "security.bad";
	char *xattr_value = (char*)kmalloc(3,GFP_KERNEL);
	// char *xattrs = (char*)kmalloc(100,GFP_KERNEL);


	/* if file is marked as bad */
	printk("amfs:file.c open, checking if file is bad\n");
	if(vfs_getxattr(file->f_path.dentry, (const char *)&"security.bad", (void *)xattr_value, 3)>0 && memcmp(xattr_value,&"yes",3)==0){
		printk("amfs:file.c, open:bad file found\n");
		err = -EACCES;
		goto out_err;
	}
	printk("amfs:file.c, open:getxattr result:%s\n",xattr_value);

	/* don't open unhashed/deleted files */
	if (d_unhashed(file->f_path.dentry)) {
		err = -ENOENT;
		goto out_err;
	}

	file->private_data =
		kzalloc(sizeof(struct amfs_file_info), GFP_KERNEL);
	if (!AMFS_F(file)) {
		err = -ENOMEM;
		goto out_err;
	}

	/* open lower object and link amfs's file struct to lower's */
	amfs_get_lower_path(file->f_path.dentry, &lower_path);
	lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
	path_put(&lower_path);
	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
		lower_file = amfs_lower_file(file);
		if (lower_file) {
			amfs_set_lower_file(file, NULL);
			fput(lower_file); /* fput calls dput for lower_dentry */
		}
	} else {
		amfs_set_lower_file(file, lower_file);
	}

	if (err)
		kfree(AMFS_F(file));
	else
		fsstack_copy_attr_all(inode, amfs_lower_inode(inode));

	/* try to set EA */
	// vfs_setxattr(file->f_path.dentry, xattr_name, (void *)&"yes", 3, XATTR_CREATE);
	// vfs_getxattr(file->f_path.dentry, xattr_name, (void *)xattr_value, 3);
	// vfs_listxattr(file->f_path.dentry, xattrs, 100);

	// printk("amfs:vfs_listxattr:%s\n",xattrs);
	// printk("amfs:vfs_getxattr: name:%s, value:%s\n",xattr_name,xattr_value);



out_err:
	// kfree(xattrs);
	kfree(xattr_value);
	return err;
}

static int amfs_flush(struct file *file, fl_owner_t id)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = amfs_lower_file(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->flush) {
		filemap_write_and_wait(file->f_mapping);
		err = lower_file->f_op->flush(lower_file, id);
	}

	return err;
}

/* release all lower object references & free the file info structure */
static int amfs_file_release(struct inode *inode, struct file *file)
{
	struct file *lower_file;

	lower_file = amfs_lower_file(file);
	if (lower_file) {
		amfs_set_lower_file(file, NULL);
		fput(lower_file);
	}

	kfree(AMFS_F(file));
	return 0;
}

static int amfs_fsync(struct file *file, loff_t start, loff_t end,
			int datasync)
{
	int err;
	struct file *lower_file;
	struct path lower_path;
	struct dentry *dentry = file->f_path.dentry;

	err = __generic_file_fsync(file, start, end, datasync);
	if (err)
		goto out;
	lower_file = amfs_lower_file(file);
	amfs_get_lower_path(dentry, &lower_path);
	err = vfs_fsync_range(lower_file, start, end, datasync);
	amfs_put_lower_path(dentry, &lower_path);
out:
	return err;
}

static int amfs_fasync(int fd, struct file *file, int flag)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = amfs_lower_file(file);
	if (lower_file->f_op && lower_file->f_op->fasync)
		err = lower_file->f_op->fasync(fd, lower_file, flag);

	return err;
}

static ssize_t amfs_aio_read(struct kiocb *iocb, const struct iovec *iov,
			       unsigned long nr_segs, loff_t pos)
{
	int err = -EINVAL;
	struct file *file, *lower_file;

	file = iocb->ki_filp;
	lower_file = amfs_lower_file(file);
	if (!lower_file->f_op->aio_read)
		goto out;
	/*
	 * It appears safe to rewrite this iocb, because in
	 * do_io_submit@fs/aio.c, iocb is a just copy from user.
	 */
	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->aio_read(iocb, iov, nr_segs, pos);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode atime as needed */
	if (err >= 0 || err == -EIOCBQUEUED)
		fsstack_copy_attr_atime(file->f_path.dentry->d_inode,
					file_inode(lower_file));
out:
	return err;
}

static ssize_t amfs_aio_write(struct kiocb *iocb, const struct iovec *iov,
				unsigned long nr_segs, loff_t pos)
{
	int err = -EINVAL;
	struct file *file, *lower_file;

	file = iocb->ki_filp;
	lower_file = amfs_lower_file(file);
	if (!lower_file->f_op->aio_write)
		goto out;
	/*
	 * It appears safe to rewrite this iocb, because in
	 * do_io_submit@fs/aio.c, iocb is a just copy from user.
	 */
	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->aio_write(iocb, iov, nr_segs, pos);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode times/sizes as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_inode_size(file->f_path.dentry->d_inode,
					file_inode(lower_file));
		fsstack_copy_attr_times(file->f_path.dentry->d_inode,
					file_inode(lower_file));
	}
out:
	return err;
}

/*
 * Amfs cannot use generic_file_llseek as ->llseek, because it would
 * only set the offset of the upper file.  So we have to implement our
 * own method to set both the upper and lower file offsets
 * consistently.
 */
static loff_t amfs_file_llseek(struct file *file, loff_t offset, int whence)
{
	int err;
	struct file *lower_file;

	err = generic_file_llseek(file, offset, whence);
	if (err < 0)
		goto out;

	lower_file = amfs_lower_file(file);
	err = generic_file_llseek(lower_file, offset, whence);

out:
	return err;
}

/*
 * Amfs read_iter, redirect modified iocb to lower read_iter
 */
ssize_t
amfs_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = amfs_lower_file(file);
	if (!lower_file->f_op->read_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->read_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode atime as needed */
	if (err >= 0 || err == -EIOCBQUEUED)
		fsstack_copy_attr_atime(file->f_path.dentry->d_inode,
					file_inode(lower_file));
out:
	return err;
}

/*
 * Amfs write_iter, redirect modified iocb to lower write_iter
 */
ssize_t
amfs_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = amfs_lower_file(file);
	if (!lower_file->f_op->write_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->write_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode times/sizes as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_inode_size(file->f_path.dentry->d_inode,
					file_inode(lower_file));
		fsstack_copy_attr_times(file->f_path.dentry->d_inode,
					file_inode(lower_file));
	}
out:
	return err;
}

const struct file_operations amfs_main_fops = {
	.llseek		= generic_file_llseek,
	.read		= amfs_read,
	.write		= amfs_write,
	.unlocked_ioctl	= amfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amfs_compat_ioctl,
#endif
	.mmap		= amfs_mmap,
	.open		= amfs_open,
	.flush		= amfs_flush,
	.release	= amfs_file_release,
	.fsync		= amfs_fsync,
	.fasync		= amfs_fasync,
	.aio_read	= amfs_aio_read,
	.aio_write	= amfs_aio_write,
	.read_iter	= amfs_read_iter,
	.write_iter	= amfs_write_iter,
};

/* trimmed directory options */
const struct file_operations amfs_dir_fops = {
	.llseek		= amfs_file_llseek,
	.read		= generic_read_dir,
	.iterate	= amfs_readdir,
	.unlocked_ioctl	= amfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amfs_compat_ioctl,
#endif
	.open		= amfs_open,
	.release	= amfs_file_release,
	.flush		= amfs_flush,
	.fsync		= amfs_fsync,
	.fasync		= amfs_fasync,
};
