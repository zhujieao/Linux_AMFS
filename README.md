# Linux_AMFS
Based on Wrapfs: A Stackable File System Interface For Linux

USAGE:
1. "cd fs/amfs/", get in the amfs directory.
2. "make", compile amfs moudule.
3. "rmmod amfs", if moudule amfs exists in system already.
4. "insmod amfs.ko", import the amfs moudule.
5. "mount -t amfs -o pattdb=PatterrnDB MounttedPath MountPath" to compelete mount.
