# arm Compiler Toolchain
BCC  ?= arm-none-eabi-gcc
BCXX ?= arm-none-eabi-g++
BLD  ?= arm-none-eabi-ld
BASM ?= unknown
# ^^ note: we will be using .S files

SUBTARGET ?= v6
$(eval $(call validate-option,SUBTARGET,v5 v6 v7))

# FREESTND_C_HDRS and FREESTND_CXX_HEADERS not defined.  However, with the toolchain we are
# using, these are actually NOT needed, since the arm-none-eabi-gcc toolchain already provides
# us with headers which should be sufficient for C++ compilation to the same degree as on
# i386 or amd64 with the specific freestanding C++ headers.

ifeq ($(SUBTARGET),v5)
	ARCH_CFLAGS = \
		-mcpu=arm926ej-s \
		-marm \
		-mfloat-abi=soft \
		-DTARGET_ARMV5
else ifeq ($(SUBTARGET),v6)
	ARCH_CFLAGS = \
		-mcpu=arm1176jzf-s \
		-marm \
		-mfloat-abi=soft \
		-DTARGET_ARMV6
else
	$(error ARMv7 not supported for now.)
endif

ARCH_LDFLAGS = \
	-z max-page-size=0x1000  \
	-L$(DDK_DIR)/../../tools \
	-lgcc-arm

LINK_ARCH = armelf
