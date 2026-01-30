#!/bin/sh
qemu-system-arm \
    -no-reboot \
    -no-shutdown \
    -d int \
    -M virt \
    -cpu arm1176 \
    -m 256M \
    -kernel ../build/arm/kernel.elf \
	-serial stdio
