# armv6 Compiler Toolchain
BCC  ?= arm-none-eabi-gcc
BCXX ?= arm-none-eabi-g++
BLD  ?= arm-none-eabi-ld
BASM ?= unknown
# ^^ note: we will be using .S files

ARCH_CFLAGS = \
	-mcpu=arm1176jzf-s \
	-marm \
	-mfloat-abi=soft \
	-DTARGET_ARM

LINK_ARCH = armelf
