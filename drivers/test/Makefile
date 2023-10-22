# Boron test driver makefile

# DEBUG flag
DEBUG ?= yes
DEBUG2 ?= no

NAME = test

# Only turn on debug2 temporarily

BUILD_DIR = build
SRC_DIR = source
INC_DIR = include
# TODO: Separate DDK includes, don't depend on kernel
#DDKI_DIR = ../../ddk/include
DDKI_DIR = ../../boron/include
SCRIPTS_DIR = scripts
LINKER_FILE = linker.ld

ARCHITECTURE=AMD64

# This is the name that our final driver executable will have.
# Change as needed.
override TARGET := $(BUILD_DIR)/out.sys

# Convenience macro to reliably declare overridable command variables.
define DEFAULT_VAR =
	ifeq ($(origin $1),default)
		override $(1) := $(2)
	endif
	ifeq ($(origin $1),undefined)
		override $(1) := $(2)
	endif
endef

ifeq ($(DEBUG), yes)
	DEFINES += -DDEBUG
endif

ifeq ($(DEBUG2), yes)
	DEFINES += -DDEBUG2
endif

# It is highly recommended to use a custom built cross toolchain to build a driver.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
$(eval $(call DEFAULT_VAR,CC,cc))
$(eval $(call DEFAULT_VAR,CXX,c++))

# Same thing for "ld" (the linker).
$(eval $(call DEFAULT_VAR,LD,ld))

# User controllable CFLAGS.
CFLAGS ?= -O2 -pipe -Wall -Wextra -DTARGET_$(ARCHITECTURE)

# User controllable CXXFLAGS.
CXXFLAGS ?= -O2 -pipe -Wall -Wextra -DTARGET_$(ARCHITECTURE)

# User controllable preprocessor flags.
CPPFLAGS ?= -I $(INC_DIR) -I $(DDKI_DIR)

# User controllable nasm flags.
NASMFLAGS ?= -F dwarf -I$(SRC_DIR) -I$(INC_DIR) -I$(DDKI_DIR)

# User controllable linker flags. We set none by default.
LDFLAGS ?=

# Internal C flags that should not be changed by the user.
override CFLAGS +=       \
	-std=c11             \
	-ffreestanding       \
	-fno-stack-protector \
	-fno-stack-check     \
	-fno-lto             \
	-fPIE                \
	-fPIC                \
	-m64                 \
	-march=x86-64        \
	-mabi=sysv           \
	-mno-80387           \
	-mno-mmx             \
	-mno-sse             \
	-mno-sse2            \
	-mno-red-zone        \
	-MMD                 \
	-MP                  \
	-I.

# Internal C++ flags that should not be changed by the user.
override CXXFLAGS +=     \
	-std=c++17           \
	-ffreestanding       \
	-fno-stack-protector \
	-fno-stack-check     \
	-fno-lto             \
	-fPIE                \
	-fPIC                \
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
	-MP                  \
	-fno-exceptions      \
	-fno-rtti            \
	-I.
	
override LDFLAGSBASE +=     \
	-nostdlib               \
	-shared -pie            \
	-m elf_x86_64           \
	-z max-page-size=0x1000

# Internal linker flags that should not be changed by the user.
override LDFLAGS +=  \
	$(LDFLAGSBASE)   \
	-T $(LINKER_FILE)

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

# Default target.
.PHONY: all
all: driver

# Link rules for the final driver executable.
$(TARGET): $(OBJ) $(LINKER_FILE)
	@echo "[LD]\tBuilding $(KERNEL)"
	@$(LD) $(OBJ) $(LDFLAGS) -o $@

# Include header dependencies.
-include $(HEADER_DEPS)

# Compilation rules for *.c files.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "[CC]\tCompiling $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Compilation rules for *.cpp files.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "[CXX]\tCompiling $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compilation rules for *.S files.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	@echo "[AS]\tCompiling $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Compilation rules for *.asm (nasm) files.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@echo "[AS]\tCompiling $<"
	@mkdir -p $(dir $@)
	@nasm $(NASMFLAGS) $< -o $@

# Remove object files and the final executable.
.PHONY: clean
clean:
	@echo "Cleaning..."
	rm -rf $(TARGET) $(OBJ) $(HEADER_DEPS)

driver: $(TARGET)
