# Boron application root makefile

# TODO: Remove the ".j" suffix (stands for job).  But it conflicts with the dir names.

# Add your libraries applications to this list.
# They will be built in this order - make sure to put libraries and
# applications their dependencies!
APPS := \
	libboron \
	libflanterm \
	libCoreGraphics \
	init \
	FullScreenTerminal \
	test

### Do not modify below ###

.PHONY: all

all:
	@for app in $(APPS); do \
		$(MAKE) -C $$app $(MAKECMDGOALS) || exit $$?; \
	done

# Other goals you might want
clean: all

