# The Boron Operating System - Makefile

# DEBUG flags
DEBUG ?= yes
DEBUG2 ?= no

# The platform we are targetting
TARGET ?= AMD64
TARGETL ?= $(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$(TARGET)))))))))))))))))))))))))))

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

APPS_LIST = \
	init

# The build directory
BUILD_DIR = build/$(TARGETL)

# The driver directory
DRIVERS_DIR = drivers

# The userland directory
USER_DIR = user

# The ISO root directory
ISO_DIR = $(BUILD_DIR)/iso_root

# The ISO target.
IMAGE_TARGET = $(BUILD_DIR)/../image.$(TARGETL).iso

KERNEL_BUILD = boron/build
SYSDLL_BUILD = borondll/build

KERNEL_ELF = $(patsubst %.elf,%.$(TARGETL).elf,$(KERNEL_BUILD)/$(KERNEL_NAME))
SYSDLL_ELF = $(patsubst %.so,%.$(TARGETL).so,$(SYSDLL_BUILD)/$(SYSDLL_NAME))

APPS_TARGETS          = $(patsubst %,$(BUILD_DIR)/%.exe,$(APPS_LIST))
APPS_DIRECTORIES      = $(patsubst %,$(USER_DIR)/%,$(APPS_LIST))
APPS_BUILD_DIRS       = $(patsubst %,%/build,$(APPS_DIRECTORIES))
DRIVERS_TARGETS       = $(patsubst %,$(BUILD_DIR)/%.sys,$(DRIVERS_LIST))
DRIVERS_DIRECTORIES   = $(patsubst %,$(DRIVERS_DIR)/%,$(DRIVERS_LIST))
DRIVERS_BUILD_DIRS    = $(patsubst %,%/build,$(DRIVERS_DIRECTORIES))
DRIVERS_LOCAL_TARGETS = $(patsubst %,%/build/out.$(TARGETL).sys,$(DRIVERS_DIRECTORIES))

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
	rm -rf $(KERNEL_BUILD) $(SYSDLL_BUILD) $(BUILD_DIR) $(DRIVERS_TARGETS) $(DRIVERS_LOCAL_TARGETS) $(DRIVERS_BUILD_DIRS) $(APPS_BUILD_DIRS)

image: limine $(IMAGE_TARGET)

$(IMAGE_TARGET): kernel borondll drivers apps limine_config
	@echo "Building iso..."
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)
	@cp $(KERNEL_ELF) $(ISO_DIR)/$(KERNEL_NAME)
	@cp $(SYSDLL_ELF) $(ISO_DIR)/$(SYSDLL_NAME)
	@cp $(DRIVERS_TARGETS) $(APPS_TARGETS) limine.cfg limine/limine-bios.sys limine/limine-bios-cd.bin $(ISO_DIR)
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

apps: $(APPS_TARGETS)

$(KERNEL_ELF): FORCE
	@mkdir -p $(BUILD_DIR)
	@echo "[MK]\tMaking $(KERNEL_ELF)"
	@$(MAKE) -C boron
	@cp $(KERNEL_ELF) $(BUILD_DIR)/$(KERNEL_NAME)

$(SYSDLL_ELF): FORCE
	@mkdir -p $(BUILD_DIR)
	@echo "[MK]\tMaking $(SYSDLL_ELF)"
	@$(MAKE) -C borondll
	@cp $(SYSDLL_ELF) $(BUILD_DIR)/$(SYSDLL_NAME)

$(BUILD_DIR)/%.exe: FORCE
	@mkdir -p $(BUILD_DIR)
	@echo "[MK]\tMaking app $(patsubst $(BUILD_DIR)/%.exe,%,$@)"
	@echo $(patsubst $(BUILD_DIR)/%.exe,$(USER_DIR)/%,$@)
	@$(MAKE) -C $(patsubst $(BUILD_DIR)/%.exe,$(USER_DIR)/%,$@)
	@cp $(USER_DIR)/$*/build/$*.$(TARGETL).exe $@

$(BUILD_DIR)/%.sys: FORCE
	@mkdir -p $(BUILD_DIR)
	@echo "[MK]\tMaking driver $(patsubst $(BUILD_DIR)/%.sys,%,$@)"
	@$(MAKE) -C $(patsubst $(BUILD_DIR)/%.sys,$(DRIVERS_DIR)/%,$@)
	@cp $(patsubst $(BUILD_DIR)/%.sys,$(DRIVERS_DIR)/%/build/out.$(TARGETL).sys,$@) $@

limine_config: limine.cfg
	@echo "[MK]\tlimine.cfg was updated"

FORCE: ;
