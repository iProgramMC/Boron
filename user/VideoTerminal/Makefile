#              The Boron Operating System
# Common makefile for all userland application targets

APP_NAME  = VideoTerminal
# DEBUG  = yes
# DEBUG2 = no

CPPFLAGS += -I../libflanterm/source -I../libCoreGraphics/include -DFLANTERM_FB_DISABLE_BUMP_ALLOC
LDFLAGS = -lflanterm -lCoreGraphics

include ../CommonMakefile
