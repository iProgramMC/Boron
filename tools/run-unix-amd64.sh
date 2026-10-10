#!/bin/sh

# NOTE: needs to be run with "make run" or "./tools/run-unix-amd64.sh" from the project root!

qemu-system-x86_64 \
    -no-reboot \
    -no-shutdown \
    -M q35 \
    -m 256M \
    -smp 4 \
    -boot d \
    -cdrom build/image.amd64.iso \
    -debugcon stdio
