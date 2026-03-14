/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/boot.c
	
Abstract:
	This module contains the bootstrap code that converts
	bootloader parameter data into kernel specific definitions.
	
Author:
	iProgramInCpp - 26 December 2025
***/
#include "../ki.h"
#include "../../mm/mi.h"

#define MAX_FRAMEBUFFER_COUNT 1

// TODO: a lot of stuff is going to be hardcoded for a start.
// hardcode properties HERE
#define MEMORY_START_ADDRESS (0x00000000)
#define MEMORY_SIZE (128*1024*1024)
static const char* KiKernelBootCmdLine = "Root=\"/InitRoot\" Init=\"/bin/Test\" NoInit=yes";

// end of hardcoded properties.

INIT void KeMarkCrashedAp(UNUSED uint32_t ProcessorIndex) {}
INIT void KeJumpstartAp(UNUSED uint32_t ProcessorIndex) {}

extern char KiKernelStart[], KiKernelEnd[], KiMemoryStart[];

LOADER_PARAMETER_BLOCK KeLoaderParameterBlock;

extern PLOADER_PARAMETER_BLOCK KiBootloaderLpb;

INIT
void KiInitLoaderParameterBlock()
{
	MiInitializeBaseIdentityMapping();
	
	KeLoaderParameterBlock = *KiBootloaderLpb;
}
