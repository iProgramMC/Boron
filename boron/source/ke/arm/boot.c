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

static int colors[] = {
	0b1111100000000000, // 0-RED
	0b1111101111100000, // 1-ORANGE
	0b1111111111100000, // 2-YELLOW
	0b1111100000011111, // 3-GREENYELLOW
	0b0000011111100000, // 4-GREEN
	0b1111100000011111, // 5-GREENBLUE
	0b0000011111111111, // 6-CYAN
	0b0000001111111111, // 7-TURQUOISE
	0b0000000000011111, // 8-BLUE
	0b1111100000011111, // 9-MAGENTA
	0b1111100000000000, // 10-RED
	0b1111101111100000, // 11-ORANGE
	0b1111111111100000, // 12-YELLOW
	0b0111111111100000, // 13-GREENYELLOW
	0b0000011111100000, // 14-GREEN
	0b0000011111101111, // 15-GREENBLUE//skipped this one!?
	0b0000011111111111, // 16-CYAN
	0b0000001111111111, // 17-TURQUOISE
	0b0000000000011111, // 18-BLUE
	0b1111100000011111, // 19-MAGENTA
};

void Marker2(int i)
{
	extern char KiRootPageTable[];
	int* a;
	if (KeGetCurrentPageTable() == (uintptr_t) &KiRootPageTable) {
		a = (int*)(0x400000+320*4*150);
	}
	else {
		a = (int*)(0xCFD00000+320*4*150);
	}
	
	int pattern = colors[i] | (colors[i] << 16);
	a[i * 4 + 3] = a[i * 4 + 2] = a[i * 4 + 1] = a[i * 4 + 0] = pattern;
	
	//const int stopAt = 7;
	//if (stopAt <= i) {
	//	while (true) {
	//		ASM("wfi");
	//	}
	//}
}

INIT
void KiInitLoaderParameterBlock()
{
	Marker2(1);
	MiInitializeBaseIdentityMapping();
	
	Marker2(2);
	KeLoaderParameterBlock = *KiBootloaderLpb;
	
	// Fix up certain addresses to be virtual.
	Marker2(3);
	PLOADER_PARAMETER_BLOCK Lpb = &KeLoaderParameterBlock;
	Marker2(4);
	FIXUP(Lpb->MemoryRegions);
	Marker2(5);
	FIXUP(Lpb->Framebuffers);
	Marker2(6);
	FIXUP(Lpb->CommandLine);
	Marker2(7);
	FIXUP(Lpb->LoaderInfo.Name);
	Marker2(8);
	FIXUP(Lpb->LoaderInfo.Version);
	Marker2(9);
	FIXUP(Lpb->ModuleInfo.List);
	Marker2(10);
	FIXUP(Lpb->ModuleInfo.Kernel.Path);
	Marker2(11);
	FIXUP(Lpb->ModuleInfo.Kernel.String);
	Marker2(12);
	FIXUP(Lpb->ModuleInfo.Kernel.Address);
	Marker2(13);
	FIXUP(Lpb->Multiprocessor.List);
	Marker2(14);
	
	for (size_t i = 0; i < Lpb->ModuleInfo.Count; i++)
	{
		FIXUP(Lpb->ModuleInfo.List[i].Path);
		FIXUP(Lpb->ModuleInfo.List[i].String);
		FIXUP(Lpb->ModuleInfo.List[i].Address);
	}
	Marker2(15);
}
