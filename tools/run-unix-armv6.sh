#!/bin/sh
qemu-system-arm \
    -no-reboot \
    -no-shutdown \
    -d int \
    -M virt \
    -cpu arm1176 \
    -m 256M \
    -kernel ../build/armv6/kernel.elf \
	-serial stdio
