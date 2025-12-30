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

// TODO: a lot of stuff is going to be hardcoded for a start.
// hardcode properties HERE
#define MEMORY_START_ADDRESS (0x00000000)
#define MEMORY_SIZE (128*1024*1024)
static const char* KiKernelBootCmdLine = "";

// end of hardcoded properties.

INIT void KeMarkCrashedAp(UNUSED uint32_t ProcessorIndex) {}
INIT void KeJumpstartAp(UNUSED uint32_t ProcessorIndex) {}

extern char KiKernelStart[], KiKernelEnd[], KiMemoryStart[];

LOADER_PARAMETER_BLOCK KeLoaderParameterBlock;

static LOADER_MEMORY_REGION KiMemoryRegions[2];
static LOADER_AP KiLoaderAp;
static void* KiLoaderApDummy;

void MiInitializeBaseIdentityMapping();

INIT
void KiInitLoaderParameterBlock()
{
	MiInitializeBaseIdentityMapping();
	
	PLOADER_MEMORY_REGION KernelRegion, FreeRegion;
	KernelRegion = &KiMemoryRegions[0];
	FreeRegion = &KiMemoryRegions[1];
	
	KernelRegion->Base = MEMORY_START_ADDRESS;
	KernelRegion->Size = (KiKernelEnd - KiKernelStart + PAGE_SIZE - 1) & (PAGE_SIZE - 1);
	KernelRegion->Type = LOADER_MEM_LOADED_PROGRAM;
	
	FreeRegion->Base = MEMORY_START_ADDRESS + KernelRegion->Size;
	FreeRegion->Size = MEMORY_SIZE - KernelRegion->Size;
	FreeRegion->Type = LOADER_MEM_FREE;
	
	KiLoaderAp.ProcessorId = 1;
	KiLoaderAp.HardwareId = 1;
	KiLoaderAp.TrampolineJumpAddress = &KiLoaderApDummy;
	KiLoaderAp.ExtraArgument = NULL;
	
	PLOADER_PARAMETER_BLOCK Lpb = &KeLoaderParameterBlock;
	
	Lpb->MemoryRegions = KiMemoryRegions;
	Lpb->MemoryRegionCount = 2;
	
	Lpb->Framebuffers = NULL;
	Lpb->FramebufferCount = 0;
	
	PLOADER_MODULE KernelModule = &Lpb->ModuleInfo.Kernel;
	KernelModule->Path = "kernel.elf";
	KernelModule->String = "";
	KernelModule->Address = KiKernelStart - 0xC0000000;
	KernelModule->Size = KiKernelEnd - KiKernelStart;
	
	Lpb->ModuleInfo.List = NULL;
	Lpb->ModuleInfo.Count = 0;
	
	Lpb->Multiprocessor.List = &KiLoaderAp;
	Lpb->Multiprocessor.Count = 1;
	Lpb->Multiprocessor.BootstrapHardwareId = 1;
	
	Lpb->LoaderInfo.Name = "fake testing armv6 loader";
	Lpb->LoaderInfo.Version = "v1.0";
	
	Lpb->CommandLine = KiKernelBootCmdLine;
}
