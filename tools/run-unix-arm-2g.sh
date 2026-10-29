#!/bin/sh

# NOTE: This runs a fork of devos50/qemu-ios.
# Download it at: https://github.com/iProgramMC/qemu-ios
#
# TODO: Allow overriding these
BORON_ROOT="$(pwd)"
QEMU_PATH="/Users/iprogramincpp/source/qemu-ios-2g"
IPOD_DATA="/Users/iprogramincpp/source/boron/keep/qemu-ios-2g-data"

QEMU_SYSTEM_ARM="$QEMU_PATH/build/qemu-system-arm"
BOOTROM="$IPOD_DATA/bootrom_240_4"
NOR="$IPOD_DATA/nor_n72ap.bin"
NAND="$IPOD_DATA/nand"
IBOOT="$IPOD_DATA/ipt_2g_openiboot.dec"
OIB_PAYLOAD="$BORON_ROOT/build/image.arm.tar"

# ~/source/qemu-ios-2g/build/arm-softmmu/qemu-system-arm \
#    -M iPod-Touch,bootrom=bootrom_240_4,nand=./nand,nor=nor_n72ap.bin \
#    -serial mon:stdio \
#    -cpu max \
#    -m 2G \
#    -d unimp

MACHINE_TYPE="iPod-Touch,bootrom=$BOOTROM,nor=$NOR,nand=$NAND,oib=$IBOOT,oib-payload=$OIB_PAYLOAD"
#,oib-elf=$OIB_ELF"

lldb -- "$QEMU_SYSTEM_ARM" \
  -M "$MACHINE_TYPE" \
  -serial stdio \
  -cpu max \
  -m 1G \
  -d unimp \
  -display sdl \
  -monitor telnet:127.0.0.1:56789,server,nowait \
  -no-reboot \
  -no-shutdown \
  -s

# -s is for initializing the GDB stub without pausing. Use '-s -S' to start
# emulation in paused mode until GDB is attached.
