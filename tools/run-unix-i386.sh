#!/bin/sh
qemu-system-i386 \
	-d int,cpu_reset -D keep/i386log.txt -s \
    -no-reboot \
    -no-shutdown \
    -M q35 \
    -m 256M \
    -boot d \
    -cdrom ./build/image.i386.iso \
    -debugcon stdio
