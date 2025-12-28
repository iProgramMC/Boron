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

ARCH_LDFLAGS = \
	-z max-page-size=0x1000  \
	-L$(DDK_DIR)/../../tools \
	-lgcc-arm

LINK_ARCH = armelf
