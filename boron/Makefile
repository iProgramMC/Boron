# Boron kernel makefile

# TODO: Have a root makefile which tackles making the image, which depends
# on another makefile which makes the kernel itself!

# DEBUG flag
DEBUG ?= yes
DEBUG2 ?= no

# debug1: general debugging text, works all the time and keeps the
#   kernel behaving how it should

# debug2: in-depth debugging, printing of page faults and stuff.
#   careful because you might get jumbled output on SMP since the
#   debug console won't lock. The advantage is that you'll be able
#   to log basically anywhere

# The build directory
BUILD_DIR = build

# The source directory
SRC_DIR = source

# The include directory
INC_DIR = include

# The scripts directory
SCRIPTS_DIR = scripts

# The linker script
LINKER_FILE = linker.ld

# The ISO root directory
ISO_DIR=$(BUILD_DIR)/iso_root

# The ISO target.
IMAGE_TARGET=$(BUILD_DIR)/image.iso

# The platform we are targetting
TARGET=AMD64

# This is the name that our final kernel executable will have.
# Change as needed.
override KERNEL := $(BUILD_DIR)/kernel.elf

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

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
$(eval $(call DEFAULT_VAR,CC,cc))
$(eval $(call DEFAULT_VAR,CXX,c++))

# Same thing for "ld" (the linker).
$(eval $(call DEFAULT_VAR,LD,ld))

# User controllable CFLAGS.
CFLAGS ?= -O2 -pipe -Wall -Wextra -I $(INC_DIR) -DTARGET_$(TARGET) $(DEFINES)

# User controllable CXXFLAGS.
CXXFLAGS ?= -O2 -pipe -Wall -Wextra -I $(INC_DIR) -DTARGET_$(TARGET) $(DEFINES)

# User controllable preprocessor flags. We set none by default.
CPPFLAGS ?=

# User controllable nasm flags.
NASMFLAGS ?= -F dwarf -I$(SRC_DIR) -I$(INC_DIR)

# User controllable linker flags. We set none by default.
LDFLAGS ?=

# Internal C flags that should not be changed by the user.
override CFLAGS +=       \
	-std=c11             \
	-ffreestanding       \
	-fno-stack-protector \
	-fno-stack-check     \
	-fno-lto             \
	-fno-pie             \
	-fno-pic             \
	-m64                 \
	-march=x86-64        \
	-mabi=sysv           \
	-mno-80387           \
	-mno-mmx             \
	-mno-sse             \
	-mno-sse2            \
	-mno-red-zone        \
	-mcmodel=kernel      \
	-MMD                 \
	-I.

# Internal C++ flags that should not be changed by the user.
override CXXFLAGS +=     \
	-std=c++17           \
	-ffreestanding       \
	-fno-stack-protector \
	-fno-stack-check     \
	-fno-lto             \
	-fno-pie             \
	-fno-pic             \
	-m64                 \
	-march=x86-64        \
	-mabi=sysv           \
	-mno-80387           \
	-mno-mmx             \
	-mno-sse             \
	-mno-sse2            \
	-mno-red-zone        \
	-mcmodel=kernel      \
	-MMD                 \
	-fno-exceptions      \
	-fno-rtti            \
	-I.
	
override LDFLAGSBASE +=     \
	-nostdlib               \
	-m elf_x86_64           \
	-z max-page-size=0x1000

# Internal linker flags that should not be changed by the user.
override LDFLAGS +=  \
	$(LDFLAGSBASE)   \
	-T $(LINKER_FILE)

# Check if the linker supports -no-pie and enable it if it does
ifeq ($(shell $(LD) --help 2>&1 | grep 'no-pie' >/dev/null 2>&1; echo $$?),0)
	override LDFLAGS += -no-pie
endif

# Internal nasm flags that should not be changed by the user.
override NASMFLAGS += \
	-f elf64

# Use find to glob all *.c, *.S, and *.asm files in the directory and extract the object names.
override CFILES := $(shell find $(SRC_DIR) -not -path '*/.*' -type f -name '*.c')
override CXXFILES := $(shell find $(SRC_DIR) -not -path '*/.*' -type f -name '*.cpp')
override ASFILES := $(shell find $(SRC_DIR) -not -path '*/.*' -type f -name '*.S')
override NASMFILES := $(shell find $(SRC_DIR) -not -path '*/.*' -type f -name '*.asm')
override OBJ := $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(CFILES:.c=.o) $(CXXFILES:.cpp=.o) $(ASFILES:.S=.o) $(NASMFILES:.asm=.o))
override HEADER_DEPS := $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(CFILES:.c=.d) $(CXXFILES:.cpp=.d) $(ASFILES:.S=.d))

KERNEL2 = $(BUILD_DIR)/kernel2.elf
KERNEL_PARTIAL = $(BUILD_DIR)/kernelp.o
SYMBOLS = $(BUILD_DIR)/_symtab.o

# Default target.
.PHONY: all
all: kernel

# Link rules for the final kernel executable.
$(KERNEL): $(SYMBOLS)
	$(LD) $(OBJ) $(SYMBOLS) $(LDFLAGS) -o $@
	
$(SYMBOLS): $(KERNEL2)
	@echo "Dumping Symbols..."
	nm -P $(KERNEL2) | $(SCRIPTS_DIR)/generate_symbols.py > $(BUILD_DIR)/_symtab.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(BUILD_DIR)/_symtab.c -o $(BUILD_DIR)/_symtab.o
	
$(KERNEL2): $(KERNEL_PARTIAL)
	$(LD) $(KERNEL_PARTIAL) -static $(LDFLAGS) -o $@

# Link rules for the amalgam object file.
$(KERNEL_PARTIAL): $(OBJ) $(LINKER_FILE)
	$(LD) -r $(OBJ) $(LDFLAGSBASE) -o $@

# Include header dependencies.
-include $(HEADER_DEPS)

# Compilation rules for *.c files.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Compilation rules for *.cpp files.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compilation rules for *.S files.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Compilation rules for *.asm (nasm) files.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	nasm $(NASMFLAGS) $< -o $@

# Remove object files and the final executable.
.PHONY: clean
clean:
	@echo "Cleaning..."
	rm -rf $(KERNEL) $(OBJ) $(HEADER_DEPS)

kernel: $(KERNEL)