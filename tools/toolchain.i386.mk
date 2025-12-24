# I386 Compiler Toolchain
BCC  ?= clang-22
BCXX ?= clang++-22
BLD  ?= ld
BASM ?= nasm

# Compiler and linker flags
ARCH_CFLAGS =        \
	-target i686-elf \
	-mno-80387       \
	-mno-mmx         \
	-mno-sse         \
	-mno-sse2
	
ARCH_LDFLAGS = \
	-z max-page-size=0x1000  \
	-L$(DDK_DIR)/../../boron \
	-lgcc-i686

ARCH_ASFLAGS = \
	-f elf32

LINK_ARCH = elf_i386
SMP = no
