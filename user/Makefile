# Boron application root makefile

# Add your libraries applications to this list.
# They will be built in this order - make sure to put libraries and
# applications *after* what they depend on!
APPS := \
	libboron \
	libflanterm \
	libCoreGraphics \
	libC++Support \
	init \
	test \
	ls \
	VideoTerminal \
	TestHarness \
	MinimalShell

# Since I need to find freestanding C or C++ headers for this platform I can't compile these yet:
ifneq ($(TARGET),ARM)
	APPS += Desktop
endif

### Do not modify below ###

.PHONY: all

all:
	@for app in $(APPS); do \
		$(MAKE) -C $$app $(MAKECMDGOALS) || exit $$?; \
	done

# Other goals you might want
clean: all

