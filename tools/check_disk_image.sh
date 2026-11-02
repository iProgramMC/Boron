#!/bin/sh

# On my disk image, the partition starts at block 63
PARTITION_START=63

if [ ! -e ./vdiske2.img ]; then
	echo "vdiske2.img is MISSING!"
	exit 1
fi

# remove temporary disk image
rm -f ./.vdiske2temp.img

dd if=./vdiske2.img of=./.vdiske2temp.img bs=512 skip="$PARTITION_START"

e2fsck ./.vdiske2temp.img

rm -f ./.vdiske2temp.img
