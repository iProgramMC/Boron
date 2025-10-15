
# Determine the build tools used automatically.
ifeq ($(TARGETL), amd64)
	# Compiler Toolchain
	BCC  ?= gcc
	BCXX ?= g++
	BLD  ?= ld
	BASM ?= nasm
	
	# Compiler and linker flags
	#
	# NOTE 7.7.2024 -- No-reorder-functions was added because a certain functions
	# was generating an "unlikely" section, which was placed at different addresses
	# in kernel.elf and kernel2.elf, screwing up the symbol table...  That's pretty
	# bad.
	#
	# TODO: fix above ^^^
	ARCH_CFLAGS =       \
		-m64            \
		-march=x86-64   \
		-mabi=sysv      \
		-mno-80387      \
		-mno-mmx        \
		-mno-sse        \
		-mno-sse2       \
		-mno-red-zone   \
		-fno-reorder-functions
	
	ARCH_LDFLAGS = \
		-z max-page-size=0x1000
	
	ARCH_ASFLAGS = \
		-f elf64
	
	LINK_ARCH = elf_x86_64
	SMP = yes
	
	ifeq ($(IS_KERNEL), yes)
		ARCH_CFLAGS += -mcmodel=kernel
	endif
	
else ifeq ($(TARGETL), i386)
	# Compiler Toolchain
	BCC  ?= clang
	BCXX ?= clang++
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
	
else
	$(error You cannot build for this architecture right now.)
endif
