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

ifeq ($(TARGET),AMD64)
DRIVERS_LIST = \
	halx86     \
	framebuf   \
	i8042prt   \
	stornvme   \
	ext2fs     \
	tmpfs      \
	test
else ifeq ($(TARGET),I386)
DRIVERS_LIST = \
	hali386    \
	framebuf   \
	i8042prt   \
	ext2fs     \
	tmpfs      \
	test
endif

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

# The init ramdisk directory.
INITRD_DIR = $(BUILD_DIR)/initrd_root

# The init ramdisk target.
INITRD_TARGET = $(BUILD_DIR)/initrd.tar

KERNEL_BUILD = boron/build
KERNEL_ELF = $(patsubst %.elf,%.$(TARGETL).elf,$(KERNEL_BUILD)/$(KERNEL_NAME))

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
	rm -rf $(KERNEL_BUILD) $(SYSDLL_BUILD) $(BUILD_DIR) $(DRIVERS_TARGETS) $(DRIVERS_LOCAL_TARGETS) $(DRIVERS_BUILD_DIRS)
	
	@echo "Cleaning user applications..."
	$(MAKE) -C user clean

image: limine $(IMAGE_TARGET) $(INITRD_TARGET)

ifeq ($(TARGET),AMD64)
include tools/build_iso_limine.mk
include tools/run_rule_amd64.mk
else ifeq ($(TARGET),I386)
include tools/build_iso_limine.mk
include tools/run_rule_i386.mk
endif

kernel: $(KERNEL_ELF)

drivers: $(DRIVERS_TARGETS)

initrd: $(INITRD_TARGET)

apps:
	$(MAKE) -C user

$(KERNEL_ELF): FORCE
	@mkdir -p $(BUILD_DIR)
	@echo "[MK]\tMaking $(KERNEL_ELF)"
	@$(MAKE) -C boron
	@cp $(KERNEL_ELF) $(BUILD_DIR)/$(KERNEL_NAME)

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

$(BUILD_DIR)/%.tar: FORCE apps
	@echo "[MK]\tBuilding initrd"
	@rm -rf $(INITRD_DIR)/usr/include
	@mkdir -p $(INITRD_DIR)/usr/include
	@cp -rf common/include $(INITRD_DIR)/usr
	@cp -rf user/include $(INITRD_DIR)/usr
	@tar -cf $@ -C $(INITRD_DIR) .

FORCE: ;
