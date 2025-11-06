#pragma once

#include <boron.h>
#include <string.h>
#include <cg/context.h>
#include <cg/color.h>
#include <cg/prims.h>

#define DEFAULT_BUFFER_SIZE (16384*128)

struct flanterm_context;

extern PGRAPHICS_CONTEXT GraphicsContext;
extern struct flanterm_context* FlantermContext;
extern HANDLE TerminalHandle, TerminalHostHandle, TerminalSessionHandle;

// Initialization
BSTATUS UseFramebuffer(const char* FramebufferPath);
BSTATUS SetupTerminal();
BSTATUS CreatePseudoterminal();
BSTATUS LaunchProcess(const char* CommandLine);
BSTATUS CreateIOLoopThreads();
BSTATUS WaitOnIOLoopThreads();

// Inter-module operation
void TerminalWrite(const char* Buffer, size_t Length);
