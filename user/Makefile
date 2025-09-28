# Boron application root makefile

# Add your applications to this list
APPS := init.j libboron.j

# Establish a dependency hierarchy
init.j: libboron.j
#someapp.j: lib1.j lib2.j lib3.j


### Do not modify below ###

.PHONY: all
all: $(APPS)

%.j:
	@$(MAKE) -C $(basename $@) $(MAKECMDGOALS)

# Other goals you might want
clean: all

