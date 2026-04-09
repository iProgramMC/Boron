# AMD64 Compiler Toolchain
BCC  ?= gcc-14
BCXX ?= g++-14
BLD  ?= ld
BASM ?= nasm

FREESTND_C_HDRS = $(REPO_ROOT)/external/freestnd-c-hdrs/x86_64/include
FREESTND_CXX_HDRS = $(REPO_ROOT)/external/freestnd-cxx-hdrs/x86_64/include

# Compiler and linker flags
#
# NOTE 7.7.2024 -- No-reorder-functions was added because a certain functions
# was generating an "unlikely" section, which was placed at different addresses
# in kernel.elf and kernel2.elf, screwing up the symbol table...  That's pretty
# bad.
#
# TODO: fix above ^^^
ARCH_CFLAGS =               \
	-m64                    \
	-march=x86-64           \
	-mabi=sysv              \
	-mno-80387              \
	-mno-mmx                \
	-mno-sse                \
	-mno-sse2               \
	-mno-red-zone           \
	-fno-reorder-functions  \
	-nostdinc               \
	-I $(FREESTND_C_HDRS)

ARCH_CXXFLAGS =             \
	-I $(FREESTND_CXX_HDRS)

ARCH_LDFLAGS = \
	-z max-page-size=0x1000

ARCH_ASFLAGS = \
	-f elf64

LINK_ARCH = elf_x86_64
SMP = yes

ifeq ($(IS_KERNEL), yes)
	ARCH_CFLAGS += -mcmodel=kernel
endif
