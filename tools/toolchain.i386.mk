# I386 Compiler Toolchain
BCC  ?= clang-22
BCXX ?= clang++-22
BLD  ?= ld
BASM ?= nasm

FREESTND_C_HDRS = $(REPO_ROOT)/external/freestnd-c-hdrs/i686/include
FREESTND_CXX_HDRS = $(REPO_ROOT)/external/freestnd-cxx-hdrs/i686/include

# Compiler and linker flags
ARCH_CFLAGS =               \
	-target i686-elf        \
	-mno-80387              \
	-mno-mmx                \
	-mno-sse                \
	-mno-sse2               \
	-nostdinc               \
	-I $(FREESTND_C_HDRS)

ARCH_CXXFLAGS =             \
	-I $(FREESTND_CXX_HDRS)

ARCH_LDFLAGS = \
	-z max-page-size=0x1000  \
	-L$(DDK_DIR)/../../tools \
	-lgcc-i686

ARCH_ASFLAGS = \
	-f elf32

LINK_ARCH = elf_i386
SMP = no
