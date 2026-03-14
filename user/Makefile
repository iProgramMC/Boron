# Boron application root makefile

# Add your libraries applications to this list.
# They will be built in this order - make sure to put libraries and
# applications *after* what they depend on!
APPS := \
	libboron \
	libflanterm \
	libCoreGraphics \
	init \
	test \
	VideoTerminal \
	TestHarness \
	MinimalShell

### Do not modify below ###

.PHONY: all

all:
	@for app in $(APPS); do \
		$(MAKE) -C $$app $(MAKECMDGOALS) || exit $$?; \
	done

# Other goals you might want
clean: all

