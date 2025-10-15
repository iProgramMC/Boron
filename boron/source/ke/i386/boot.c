/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/i386/boot.c
	
Abstract:
	This module contains the bootstrap code that converts
	Multiboot bootloader data into kernel specific definitions.
	
Author:
	iProgramInCpp - 15 October 2025
***/
#include "../ki.h"
#include "../../mm/mi.h"
#include "mboot.h"

extern uint32_t KiMultibootSignature;
extern multiboot_info_t* KiMultibootPointer;

#define P2V(address) ((void*)(MI_IDENTMAP_START + address))

LOADER_PARAMETER_BLOCK KeLoaderParameterBlock;

INIT void KeMarkCrashedAp(UNUSED uint32_t ProcessorIndex) {}
INIT void KeJumpstartAp(UNUSED uint32_t ProcessorIndex) {}

#define MAX_MEMORY_REGIONS 256
static LOADER_MEMORY_REGION KiMemoryRegions[MAX_MEMORY_REGIONS];

// Allocate a contiguous area of memory from the memory map.
INIT
static void* KiEarlyAllocateMemoryFromMemMap(size_t Size)
{
	Size = (Size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	
	PLOADER_PARAMETER_BLOCK Lpb = &KeLoaderParameterBlock;
	for (uint64_t i = 0; i < Lpb->MemoryRegionCount; i++)
	{
		// if the entry isn't usable, skip it
		PLOADER_MEMORY_REGION Entry = &Lpb->MemoryRegions[i];
		
		if (Entry->Type != LOADER_MEM_FREE)
			continue;
		
		// Note: Usable entries are guaranteed to be aligned to page size, and
		// not overlap any other entries.
		if (Entry->Size < Size)
			continue;
		
		uintptr_t CurrAddr = Entry->Base;
		Entry->Base += Size;
		Entry->Size -= Size;
		
		return (void*) MmGetHHDMOffsetAddr(CurrAddr);
	}
	
	KeCrashBeforeSMPInit("Error, out of memory in KiEarlyAllocateMemoryFromMemMap");
}

INIT
static void KiRemoveAreaFromMemMap(uintptr_t StartAddress, size_t Size)
{
	// TODO
	(void) StartAddress;
	(void) Size;
}


INIT
void KiInitLoaderParameterBlock()
{
	// Initialize the base identity mapping.
	MiInitializeBaseIdentityMapping();
	
	PLOADER_PARAMETER_BLOCK Lpb = &KeLoaderParameterBlock;
	
	if (KiMultibootSignature != MULTIBOOT_BOOTLOADER_MAGIC)
		KeCrashBeforeSMPInit("KiMultibootSignature is not %08x, it's %08x!", MULTIBOOT_BOOTLOADER_MAGIC, KiMultibootSignature);
	
	// Initialize the memory regions.
	
	// Initialize the kernel module.
	
	// Initialize the other modules.
	
	// Initialize the CPUs.
	
	// Initialize the frame buffers.
	
	// Initialize the bootloader's information as well as the command line.
	
	
}
