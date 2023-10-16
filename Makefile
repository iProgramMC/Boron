# Boron kernel makefile

# TODO: Have a root makefile which tackles making the image, which depends
# on another makefile which makes the kernel itself!

# DEBUG flag
DEBUG ?= yes
DEBUG2 ?= no

# List of things to build:
#  KERNEL - Self explanatory
#  LOADER - OS loader
KERNEL_NAME = kernel.elf
LOADER_NAME = boronldr.elf

# debug1: general debugging text, works all the time and keeps the
#   kernel behaving how it should

# debug2: in-depth debugging, printing of page faults and stuff.
#   careful because you might get jumbled output on SMP since the
#   debug console won't lock. The advantage is that you'll be able
#   to log basically anywhere

# The build directory
BUILD_DIR = build

# The ISO root directory
ISO_DIR=$(BUILD_DIR)/iso_root

# The ISO target.
IMAGE_TARGET=$(BUILD_DIR)/image.iso

# The platform we are targetting
TARGET=AMD64

KERNEL_BUILD=boron/build
LOADER_BUILD=loader/build

KERNEL_ELF=$(KERNEL_BUILD)/$(KERNEL_NAME)
KERNEL_DELF=$(BUILD_DIR)/$(KERNEL_NAME)
LOADER_ELF=$(LOADER_BUILD)/$(LOADER_NAME)
LOADER_DELF=$(BUILD_DIR)/$(LOADER_NAME)

# Convenience macro to reliably declare overridable command variables.
define DEFAULT_VAR =
	ifeq ($(origin $1),default)
		override $(1) := $(2)
	endif
	ifeq ($(origin $1),undefined)
		override $(1) := $(2)
	endif
endef

# Default target.
.PHONY: all
all: image

# Remove object files and the final executable.
.PHONY: clean
clean:
	@echo "Cleaning..."
	rm -rf $(KERNEL_BUILD) $(LOADER_BUILD) $(BUILD_DIR)

image: limine $(IMAGE_TARGET)

$(IMAGE_TARGET): kernel loader
	@echo "Building iso..."
	@mkdir -p $(dir $(KERNEL_DELF))
	@mkdir -p $(dir $(LOADER_DELF))
	@cp $(KERNEL_ELF) $(KERNEL_DELF)
	@cp $(LOADER_ELF) $(LOADER_DELF)
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)
	@cp $(KERNEL_DELF) $(LOADER_DELF) limine.cfg limine/limine.sys limine/limine-cd.bin $(ISO_DIR)
	@xorriso -as mkisofs -b limine-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --protective-msdos-label $(ISO_DIR) -o $@ 2>/dev/null
	@limine/limine-deploy $@ 2>/dev/null
	@rm -rf $(ISO_DIR)

run: image
	@echo "Running..."
	@./run-unix.sh

runw: image
	@echo "Invoking WSL to run the OS..."
	@./run.sh

kernel: $(KERNEL_ELF)

$(KERNEL_ELF):
	$(MAKE) -C boron

loader: $(LOADER_ELF)

$(LOADER_ELF):
	$(MAKE) -C loader
