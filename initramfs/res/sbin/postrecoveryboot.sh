#!/sbin/sh

# Restart with root hacked adbd
touch /tmp/recovery.log
sync
cat /proc/kmsg
ls -l /dev/block
