# Linux_AMFS
Based on Wrapfs: A Stackable File System Interface For Linux

USAGE:

after building and installing this kernel with the .config file in root folder,

1. "cd fs/amfs/", get in the amfs directory.

2. "make", compile amfs moudule.

3. "rmmod amfs", if moudule amfs exists in system already.

4. "insmod amfs.ko", import the amfs moudule.

5. "mount -t amfs -o pattdb=PatterrnDB MounttedPath MountPath" to compelete mount.

6. "./amfsctl -l /mnt/amfs" will list all patterns stored in super_block.

7. "./amfsctl -a "virusPattern" /mnt/amfs" will add a patterns to super_block.

8. "./amfsctl -r "virusPattern" /mnt/amfs" will remove all patterns matches "virusPattern" exactly.

9. "umount /mnt/amfs" to unmount.

* make sure PatternDB is not in MounttedPath, otherwise mounting will not succeed.

* MountPath should also exits before mounting.

* an example "mount -t amfs -o pattdb=/mypatterns.db /usr/src /mnt/amfs"


