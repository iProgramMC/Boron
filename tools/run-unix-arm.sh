#!/bin/sh

# NOTE: This runs a fork of devos50/qemu-ios.
# Download it at: https://github.com/iProgramMC/qemu-ios
#
# TODO: Allow overriding these
BORON_ROOT="$(pwd)"
QEMU_PATH="/Users/iprogramincpp/source/qemu-ios"
IPOD_DATA="/Users/iprogramincpp/source/boron/keep/qemu-ios-data"

QEMU_SYSTEM_ARM="$QEMU_PATH/build/arm-softmmu/qemu-system-arm"
BOOTROM="$IPOD_DATA/bootrom_s5l8900"
NOR="$IPOD_DATA/nor_n45ap.bin"
NAND="$IPOD_DATA/nand.img"
IBOOT="$IPOD_DATA/ipt_1g_openiboot.dec"
OIB_ELF="$BORON_ROOT/build/image.arm.tar"

MACHINE_TYPE="iPod-Touch,bootrom=$BOOTROM,iboot=$IBOOT,nand=$NAND,oib-elf=$OIB_ELF"

"$QEMU_SYSTEM_ARM" \
  -M "$MACHINE_TYPE" \
  -serial stdio \
  -cpu max \
  -m 1G \
  -d unimp \
  -pflash "$NOR" \
  -display sdl \
  -monitor telnet:127.0.0.1:56789,server,nowait \
  -no-reboot \
  -no-shutdown \
  -d int \
  -D "$BORON_ROOT/keep/armlog.txt" \
  -s

# -s is for initializing the GDB stub without pausing. Use '-s -S' to start
# emulation in paused mode until GDB is attached.
