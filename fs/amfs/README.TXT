USAGE:
1. "cd hw2-jieazhu/fs/amfs/" to get in the amfs directory.
2. run command "make" to compile amfs moudule.
3. run "rmmod amfs" if moudule amfs exists in system already.
4. run "insmod amfs.ko" to import the new moudule.
5. "mount -t amfs -o pattdb=PatterrnDB MounttedPath MountPath" to compelete mount.
* make sure PatternDB is not in MounttedPath, otherwise mounting will not succeed.
* MountPath should also exits before mounting.
* an example "mount -t amfs -o pattdb=/mypatterns.db /usr/src /mnt/amfs"
6. "./amfsctl -l /mnt/amfs" will list all patterns stored in super_block.
7. "./amfsctl -a "virusPattern" /mnt/amfs" will add a patterns to super_block.
8. "./amfsctl -r "virusPattern" /mnt/amfs" will remove all patterns matches "virusPattern" exactly.
9. "umount /mnt/amfs" to unmount.

There are some mechanisms to protect PatternDB, mark file as bad, check if PatternDB is in MounttedPath, and so no:
1. The amfs will first check if PatternDB, MounttedPath and MountPath's existence, 
then compare the absolute path of PatternDB, MounttedPath to make sure that PatternDB is out of MounttedPath.

2. Then in amfs_read_super in main.c, the patterns will be read and stored in the private data of super_block as a list.
PatternDB's absolute path will also be stored so that we can sync changes in super_block to disk.
There is a member called modify_count, whenever a ./amfsctl -a or -r is executed, this count will be plused 1,
and patterns in super_block will write to disk when it comes to 10, then make it 0, below is part of members in super_block:
/* amfs super-block data in memory */
struct amfs_sb_info {
    struct super_block *lower_sb;
    struct pattern_node *pattern_head;
    char *pattern_db_path;
    int modify_count;
};

3. In the end of mounting, PatternDB's permission will be set to prevent from accessing by users, but root can still access it.
So I change the i_size of PatternDB.s inode to 0, to mark the DB file as empty.
When power is suddenly off, system will read a correct inode size from disk, so that the patternDB will not be lost.
This is not am elegant way to protect PatternDB, but I do not figure out a good way to protect DB without lost it after power off.
Also, the DB's permission and size will be stored in super_block, and restored when unmounting.

4. ./amfsctl -l, -a, -r will access the list in the super block.

5. While reading or writing, amfs will check the buf, set the EA "security.bad" as true if part or all buf matches a pattern.
EA will be checked before open, read, write a file.
The bad file's permission will also changes just like what we have done when protecting DB file, so that only root can delete it.
