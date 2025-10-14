# Boron kernel makefile

# DEBUG flag
DEBUG ?= yes
DEBUG2 ?= no
DWARF_SYMBOLS ?= yes

# Major and minor versions are incremented manually, but the build number
# is incremented automatically upon a successful link.
VER_MAJOR = 0
VER_MINOR = 0

# debug1: general debugging text, works all the time and keeps the
#   kernel behaving how it should

# debug2: in-depth debugging, printing of page faults and stuff.
#   careful because you might get jumbled output on SMP since the
#   debug console won't lock. The advantage is that you'll be able
#   to log basically anywhere

# SECURE flag.
# The SECURE flag enables additional security features if desired.
# Currently the only application is clearing the input buffers with zero once read.
SECURE ?= no

# Default Target
TARGET ?= AMD64

# This sucks.
TARGETL=$(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$(TARGET)))))))))))))))))))))))))))

BUILD_DIR = build
SRC_DIR = source
INC_DIR = include
DDKI_DIR = ../common/include
SCRIPTS_DIR = scripts
LINKER_FILE = linker.$(TARGETL).ld
ISO_DIR=$(BUILD_DIR)/iso_root
IMAGE_TARGET=$(BUILD_DIR)/image.iso

# This is the name that our final kernel executable will have.
# Change as needed.
override KERNEL := $(BUILD_DIR)/kernel.$(TARGETL).elf

# Convenience macro to reliably declare overridable command variables.
define DEFAULT_VAR =
	ifeq ($(origin $1),default)
		override $(1) := $(2)
	endif
	ifeq ($(origin $1),undefined)
		override $(1) := $(2)
	endif
endef

# Defines - I would edit flanterm itself but then submodules would get rid of our work.
DEFINES = -DFLANTERM_FB_DISABLE_CANVAS -DFLANTERM_FB_DISABLE_BUMP_ALLOC

ifeq ($(DEBUG), yes)
	DEFINES += -DDEBUG
endif

ifeq ($(DEBUG2), yes)
	DEFINES += -DDEBUG2
endif

ifeq ($(SECURE), yes)
	DEFINES += -DSECURE
endif

# Note: DWARF symbols aren't really reliable on -O2, so best to use no optimization
# while using them.
ifeq ($(DWARF_SYMBOLS), yes)
	DEFINES += -gdwarf-4 -O0
else
	DEFINES += -O3
endif

# Determine the build tools used automatically.
ifeq ($(TARGETL), amd64)
	# Compiler Toolchain
	BCC  ?= gcc
	BCXX ?= g++
	BLD  ?= ld
	BASM ?= nasm
	
	# Compiler and linker flags
	ARCH_CFLAGS =       \
		-m64            \
		-march=x86-64   \
		-mabi=sysv      \
		-mno-80387      \
		-mno-mmx        \
		-mno-sse        \
		-mno-sse2       \
		-mno-red-zone   \
		-mcmodel=kernel
		
	ARCH_LDFLAGS = \
		-m elf_x86_64 \
		-z max-page-size=0x1000
	
	ARCH_ASFLAGS = \
		-f elf64
	
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
		-z max-page-size=0x1000
	
	ARCH_ASFLAGS = \
		-f elf32
	
else
	$(error You cannot build for this architecture right now.)
endif

# User controllable CFLAGS.
CFLAGS ?= -pipe -Wall -Wextra -DTARGET_$(TARGET) -DKERNEL -DIS_KERNEL_MODE $(DEFINES)

# User controllable CXXFLAGS.
CXXFLAGS ?= -pipe -Wall -Wextra -DTARGET_$(TARGET) -DKERNEL -DIS_KERNEL_MODE $(DEFINES)

# User controllable preprocessor flags.
CPPFLAGS ?= -I $(INC_DIR) -I $(DDKI_DIR)

# User controllable nasm flags.
NASMFLAGS ?= -F dwarf -I$(SRC_DIR) -I$(INC_DIR) -I$(DDKI_DIR)

# User controllable linker flags. We set none by default.
LDFLAGS ?=

# Internal C flags that should not be changed by the user.
#
# NOTE 7.7.2024 -- No-reorder-functions was added because a certain functions
# was generating an "unlikely" section, which was placed at different addresses
# in kernel.elf and kernel2.elf, screwing up the symbol table...  That's pretty
# bad.
#
# TODO: fix above ^^^
CFLAGS +=                   \
	-fno-omit-frame-pointer \
	-std=c11                \
	-ffreestanding          \
	-fno-stack-protector    \
	-fno-stack-check        \
	-fno-lto                \
	-fno-pie                \
	-fno-pic                \
	-fno-reorder-functions  \
	-m64                    \
	-march=x86-64           \
	-mabi=sysv              \
	-mno-80387              \
	-mno-mmx                \
	-mno-sse                \
	-mno-sse2               \
	-mno-red-zone           \
	-mcmodel=kernel         \
	-MMD                    \
	-MP                     \
	-I.                     \
	$(ARCH_CFLAGS)

# Internal C++ flags that should not be changed by the user.
CXXFLAGS +=                 \
	-fno-omit-frame-pointer \
	-std=c++17              \
	-ffreestanding          \
	-fno-stack-protector    \
	-fno-stack-check        \
	-fno-lto                \
	-fno-pie                \
	-fno-pic                \
	-fno-reorder-functions  \
	-MMD                    \
	-MP                     \
	-fno-exceptions         \
	-fno-rtti               \
	-I.                     \
	$(ARCH_CFLAGS)
	
LDFLAGSBASE +=              \
	-nostdlib               \
	$(ARCH_LDFLAGS)

# Internal linker flags that should not be changed by the user.
LDFLAGS +=           \
	$(LDFLAGSBASE)   \
	-T $(LINKER_FILE)

# Check if the linker supports -no-pie and enable it if it does
ifeq ($(shell $(LD) --help 2>&1 | grep 'no-pie' >/dev/null 2>&1; echo $$?),0)
	override LDFLAGS += -no-pie
endif

# Internal nasm flags that should not be changed by the user.
NASMFLAGS += \
	-f elf64

# Use find to glob all *.c, *.S, and *.asm files in the directory and extract the object names.
EXCLUDE_WRONG_ARCH = '(' '(' -path 'source/ke/$(TARGETL)/*' ')' -o '(' -path 'source/mm/$(TARGETL)/*' ')' -o '(' -not -path 'source/ke/*/*' -not -path 'source/mm/*/*' ')' ')'
override CFILES      := $(shell find $(SRC_DIR) -not -path '*/.*' $(EXCLUDE_WRONG_ARCH) -type f -name '*.c')
override CXXFILES    := $(shell find $(SRC_DIR) -not -path '*/.*' $(EXCLUDE_WRONG_ARCH) -type f -name '*.cpp')
override ASFILES     := $(shell find $(SRC_DIR) -not -path '*/.*' $(EXCLUDE_WRONG_ARCH) -type f -name '*.S')
override NASMFILES   := $(shell find $(SRC_DIR) -not -path '*/.*' $(EXCLUDE_WRONG_ARCH) -type f -name '*.asm')
override OBJ         := $(patsubst %.o,%.$(TARGETL).o,$(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(CFILES:.c=.o) $(CXXFILES:.cpp=.o) $(ASFILES:.S=.o) $(NASMFILES:.asm=.o)))
override HEADER_DEPS := $(patsubst %.o,%.d,$(OBJ))

# It's kind of a mess, but basically, here's what everything does:
# kernel2.elf - Kernel.elf without the symbol table
# kernelp.o   - The entire kernel as an object file, so that we don't have to link _everything_ twice
# _symtab.o   - The compiled symbol table (currently, only .text) for the kernel.
KERNEL2        = $(BUILD_DIR)/kernel2.$(TARGETL).elf
KERNEL_PARTIAL = $(BUILD_DIR)/kernelp.$(TARGETL).o
SYMBOLS        = $(BUILD_DIR)/_symtab.$(TARGETL).o

# Default target.
.PHONY: all
all: kernel

BUILD_NUMBER_FILE = $(SRC_DIR)/build_number
VER_BUILD = $(shell cat $(BUILD_NUMBER_FILE))

CFLAGS += -D__BORON_MAJOR=$(VER_MAJOR) -D__BORON_MINOR=$(VER_MINOR) -D__BORON_BUILD=$(VER_BUILD)

# Link rules for the final kernel executable.
$(KERNEL): $(SYMBOLS)
	@echo "[LD]\tBuilding $(KERNEL)"
	@$(BLD) $(OBJ) $(SYMBOLS) $(LDFLAGS) -o $@
	
$(SYMBOLS): $(KERNEL2)
	@echo "[NM]\tDumping and compiling symbols"
	@nm -P $(KERNEL2) | $(SCRIPTS_DIR)/generate_symbols.py > $(BUILD_DIR)/_symtab.$(TARGETL).asm
	@$(BASM) $(NASMFLAGS) $(BUILD_DIR)/_symtab.$(TARGETL).asm -o $(BUILD_DIR)/_symtab.$(TARGETL).o
	
$(KERNEL2): $(KERNEL_PARTIAL)
	@echo "[LD]\tLinking kernel to extract symbols"
	@$(BLD) $(KERNEL_PARTIAL) -static $(LDFLAGS) -o $@

# Link rules for the amalgam object file.
$(KERNEL_PARTIAL): $(OBJ) $(LINKER_FILE)
	@echo "[LD]\tPartially linking kernel"
	@$(BLD) -r $(OBJ) $(LDFLAGSBASE) -o $@

$(BUILD_DIR)/ke/version.$(TARGETL).o: $(filter-out $(BUILD_DIR)/ke/version.$(TARGETL).o, $(OBJ)) $(SRC_DIR)/ke/version.c
	@echo "[CC]\tCompiling $(SRC_DIR)/ke/version.c"
	@mkdir -p $(dir $@)
	@$(BCC) $(CPPFLAGS) $(CFLAGS) -c $(SRC_DIR)/ke/version.c -o $@
	@echo "[MK]\tIncrementing build number"
	@echo $$(($(VER_BUILD) + 1)) > $(BUILD_NUMBER_FILE)

# Include header dependencies.
-include $(HEADER_DEPS)

# Compilation rules for *.c files.
$(BUILD_DIR)/%.$(TARGETL).o: $(SRC_DIR)/%.c
	@echo "[CC]\tCompiling $<"
	@mkdir -p $(dir $@)
	@$(BCC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Compilation rules for *.cpp files.
$(BUILD_DIR)/%.$(TARGETL).o: $(SRC_DIR)/%.cpp
	@echo "[CXX]\tCompiling $<"
	@mkdir -p $(dir $@)
	@$(BCXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compilation rules for *.S files.
$(BUILD_DIR)/%.$(TARGETL).o: $(SRC_DIR)/%.S
	@echo "[AS]\tCompiling $<"
	@mkdir -p $(dir $@)
	@$(BCC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Compilation rules for *.asm (nasm) files.
$(BUILD_DIR)/%.$(TARGETL).o: $(SRC_DIR)/%.asm
	@echo "[AS]\tCompiling $<"
	@mkdir -p $(dir $@)
	@$(BASM) $(NASMFLAGS) $< -o $@

# Remove object files and the final executable.
.PHONY: clean
clean:
	@echo "Cleaning..."
	@rm -rf $(KERNEL) $(OBJ) $(HEADER_DEPS)

kernel: $(KERNEL)
