#pragma once

#include <boron.h>
#include <string.h>
#include <cg/context.h>
#include <cg/prims.h>

extern PGRAPHICS_CONTEXT GraphicsContext;

BSTATUS UseFramebuffer(const char* FramebufferPath);
BSTATUS SetupTerminal();
BSTATUS CreatePseudoterminal();
BSTATUS LaunchProcess(const char* CommandLine);
void UpdateLoop();
