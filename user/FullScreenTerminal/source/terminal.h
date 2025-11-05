#pragma once

#include <boron.h>
#include <string.h>
#include <cg/context.h>
#include <cg/color.h>
#include <cg/prims.h>

struct flanterm_context;

extern PGRAPHICS_CONTEXT GraphicsContext;
extern struct flanterm_context* FlantermContext;

BSTATUS UseFramebuffer(const char* FramebufferPath);
BSTATUS SetupTerminal();
BSTATUS CreatePseudoterminal();
BSTATUS LaunchProcess(const char* CommandLine);
void UpdateLoop();
