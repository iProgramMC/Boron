#!/bin/sh

# NOTE: this is outdated. DO NOT USE THIS!

qemu-system-arm \
    -no-reboot \
    -no-shutdown \
    -d int \
    -M virt \
    -cpu arm1176 \
    -m 256M \
    -kernel ../build/arm/kernel.elf \
	-serial stdio
