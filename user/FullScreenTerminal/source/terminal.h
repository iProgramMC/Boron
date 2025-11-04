#pragma once

#include <boron.h>
#include <string.h>

extern IOCTL_FRAMEBUFFER_INFO FramebufferInfo;
extern void* FramebufferMapAddress;

BSTATUS UseFramebuffer(const char* FramebufferPath);

BSTATUS SetupTerminal();
BSTATUS CreatePseudoterminal();
BSTATUS LaunchProcess(const char* CommandLine);
void UpdateLoop();
