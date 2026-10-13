#!/bin/sh

# NOTE: needs to be run with "make run" or "./tools/run-unix-amd64.sh" from the project root!

DRIVE_SPECIFICATION=""
if [ -e ./vdiske2.img ]; then
	DRIVE_SPECIFICATION="-drive id=nvm,file=./vdiske2.img,if=none,format=raw -device nvme,serial=deadbeef,drive=nvm"
fi

qemu-system-x86_64 \
    -no-reboot \
    -no-shutdown \
    -M q35 \
    -m 256M \
    -smp 4 \
    -boot d \
    -cdrom build/image.amd64.iso \
    -debugcon stdio \
    $DRIVE_SPECIFICATION
