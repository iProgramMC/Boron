# The Boron Operating System - Makefile

# DEBUG flags
DEBUG ?= yes
DEBUG2 ?= no

# The platform we are targetting
TARGET=AMD64

# debug1: general debugging text, works all the time and keeps the
#   kernel behaving how it should

# debug2: in-depth debugging, printing of page faults and stuff.
#   careful because you might get jumbled output on SMP since the
#   debug console won't lock. The advantage is that you'll be able
#   to log basically anywhere

KERNEL_NAME = kernel.elf
SYSDLL_NAME = libboron.so

DRIVERS_LIST = \
	halx86     \
	i8042prt   \
	stornvme   \
	ext2fs     \
	test

# The build directory
BUILD_DIR = build

# The driver directory
DRIVERS_DIR = drivers

# The ISO root directory
ISO_DIR=$(BUILD_DIR)/iso_root

# The ISO target.
IMAGE_TARGET=$(BUILD_DIR)/image.iso

KERNEL_BUILD=boron/build
SYSDLL_BUILD=borondll/build

KERNEL_ELF=$(KERNEL_BUILD)/$(KERNEL_NAME)
SYSDLL_ELF=$(SYSDLL_BUILD)/$(SYSDLL_NAME)

DRIVERS_TARGETS       = $(patsubst %,$(BUILD_DIR)/%.sys,$(DRIVERS_LIST))
DRIVERS_DIRECTORIES   = $(patsubst %,$(DRIVERS_DIR)/%,$(DRIVERS_LIST))
DRIVERS_BUILD_DIRS    = $(patsubst %,%/build,$(DRIVERS_DIRECTORIES))
DRIVERS_LOCAL_TARGETS = $(patsubst %,%/build/out.sys,$(DRIVERS_DIRECTORIES))

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
	rm -rf $(KERNEL_BUILD) $(SYSDLL_BUILD) $(BUILD_DIR) $(DRIVERS_TARGETS) $(DRIVERS_LOCAL_TARGETS) $(DRIVERS_BUILD_DIRS)

image: limine $(IMAGE_TARGET)

$(IMAGE_TARGET): kernel borondll drivers limine_config
	@echo "Building iso..."
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)
	@cp $(KERNEL_ELF) $(SYSDLL_ELF) $(DRIVERS_TARGETS) limine.cfg limine/limine-bios.sys limine/limine-bios-cd.bin $(ISO_DIR)
	@xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --protective-msdos-label $(ISO_DIR) -o $@ 2>/dev/null
	@limine/limine-deploy $@ 2>/dev/null
	@rm -rf $(ISO_DIR)

run: image
	@echo "Running..."
	@./run-unix.sh

runw: image
	@echo "Invoking WSL to run the OS..."
	@./run.sh

kernel: $(KERNEL_ELF)

borondll: $(SYSDLL_ELF)

drivers: $(DRIVERS_TARGETS)

$(KERNEL_ELF): FORCE
	@echo "[MK]\tMaking $(KERNEL_ELF)"
	@$(MAKE) -C boron

$(SYSDLL_ELF): FORCE
	@echo "[MK]\tMaking $(SYSDLL_ELF)"
	@$(MAKE) -C borondll

# note: the $(dir $(dir)) is kind of cheating but it works!
$(BUILD_DIR)/%.sys: FORCE
	@echo "[MK]\tMaking driver $(patsubst $(BUILD_DIR)/%.sys,%,$@)"
	@$(MAKE) -C $(patsubst $(BUILD_DIR)/%.sys,$(DRIVERS_DIR)/%,$@)
	@mkdir -p $(dir $@)
	@cp $(patsubst $(BUILD_DIR)/%.sys,$(DRIVERS_DIR)/%/build/out.sys,$@) $@

limine_config: limine.cfg
	@echo "[MK]\tlimine.cfg was updated"

FORCE: ;
