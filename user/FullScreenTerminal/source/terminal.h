#pragma once

#include <boron.h>
#include <string.h>

extern IOCTL_FRAMEBUFFER_INFO FbInfo;
extern uint8_t* FbAddress;

BSTATUS UseFramebuffer(const char* FramebufferPath);

BSTATUS SetupTerminal();
BSTATUS CreatePseudoterminal();
BSTATUS LaunchProcess(const char* CommandLine);
void UpdateLoop();
