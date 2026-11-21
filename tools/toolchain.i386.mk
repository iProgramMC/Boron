# I386 Compiler Toolchain

HOST_ARCH = $(shell uname -m)
HOST_SYSTEM = $(shell uname -s)
ifeq ($(HOST_ARCH),x86_64)
	BLD ?= ld
else
	BLD ?= ld.lld
endif

# Apple clang doesn't compile i386 builds correctly at the moment.
# Have to use Homebrew clang instead.
ifeq ($(HOST_SYSTEM),Darwin)
	CLANG_PFX ?= /opt/homebrew/opt/llvm/bin/
	CLANG_SFX ?=
else
	CLANG_PFX ?=
	CLANG_SFX ?=
endif

BCC  ?= $(CLANG_PFX)clang$(CLANG_SFX)
BCXX ?= $(CLANG_PFX)clang++$(CLANG_SFX)
BASM ?= nasm

# We are not using a freestanding toolchain (e.g. i?86-elf-gcc) (although we really should),
# so use these as an alternative.
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
	-lgcc-i686               \
	--hash-style=sysv

ARCH_ASFLAGS = \
	-f elf32

LINK_ARCH = elf_i386
SMP = no
