#pragma once

#include <boron.h>
#include <string.h>
#include <cg/context.h>
#include <cg/color.h>
#include <cg/prims.h>

#define DEFAULT_BUFFER_SIZE (16384)

struct flanterm_context;

extern struct flanterm_context* FlantermContext;
extern PGRAPHICS_CONTEXT GraphicsContext;
extern HANDLE TerminalHandle, TerminalHostHandle, TerminalSessionHandle;
extern HANDLE KeyboardHandle;

// Initialization
BSTATUS UseFramebuffer(const char* FramebufferPath);
BSTATUS UseKeyboard(const char* KeyboardName);
BSTATUS SetupTerminal();
BSTATUS CreatePseudoterminal();
BSTATUS LaunchProcess(const char* FileName, const char* Arguments);
BSTATUS CreateIOLoopThreads();
BSTATUS WaitOnIOLoopThreads();

// Inter-module operation
void TranslateKeyCode(char* Input, uint8_t KeyCode);
void TerminalWrite(const char* Buffer, size_t Length);
