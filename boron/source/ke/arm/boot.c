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

INIT void KeMarkCrashedAp(UNUSED uint32_t ProcessorIndex) {}
INIT void KeJumpstartAp(UNUSED uint32_t ProcessorIndex) {}

extern char KiKernelStart[], KiKernelEnd[], KiMemoryStart[];

LOADER_PARAMETER_BLOCK KeLoaderParameterBlock;

extern PLOADER_PARAMETER_BLOCK KiBootloaderLpb;

#define P2V(Ptr) ((Ptr) ? (void*)((uintptr_t)(Ptr) + 0xC0000000) : NULL)
#define FIXUP(Addr) ((Addr) = P2V(Addr))

INIT
void KiInitLoaderParameterBlock()
{
	MiInitializeBaseIdentityMapping();
	
	PLOADER_PARAMETER_BLOCK LpbSrc = P2V(KiBootloaderLpb);
	KeLoaderParameterBlock = *LpbSrc;
	
	// Fix up certain addresses to be virtual.
	PLOADER_PARAMETER_BLOCK Lpb = &KeLoaderParameterBlock;
	FIXUP(Lpb->MemoryRegions);
	FIXUP(Lpb->Framebuffers);
	FIXUP(Lpb->CommandLine);
	FIXUP(Lpb->LoaderInfo.Name);
	FIXUP(Lpb->LoaderInfo.Version);
	FIXUP(Lpb->ModuleInfo.List);
	FIXUP(Lpb->ModuleInfo.Kernel.Path);
	FIXUP(Lpb->ModuleInfo.Kernel.String);
	FIXUP(Lpb->ModuleInfo.Kernel.Address);
	FIXUP(Lpb->Multiprocessor.List);
	
	for (size_t i = 0; i < Lpb->ModuleInfo.Count; i++)
	{
		FIXUP(Lpb->ModuleInfo.List[i].Path);
		FIXUP(Lpb->ModuleInfo.List[i].String);
		FIXUP(Lpb->ModuleInfo.List[i].Address);
	}
}
