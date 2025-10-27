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
extern multiboot_info_t* KiMultibootInfo;

#define P2V(address) ((void*)(MI_IDENTMAP_START + (uintptr_t)(address)))

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
		
		ASSERT(CurrAddr < MI_IDENTMAP_SIZE);
		return (void*) (MI_IDENTMAP_START + CurrAddr);
	}
	
	KeCrashBeforeSMPInit("Error, out of memory in KiEarlyAllocateMemoryFromMemMap");
}

INIT
static void KiRemoveAreaFromMemMap(uintptr_t StartAddress, size_t Size)
{
	PLOADER_PARAMETER_BLOCK Lpb = &KeLoaderParameterBlock;
	
	// Ensure the start address and size greedily cover page boundaries.
	Size += StartAddress & (PAGE_SIZE - 1);
	StartAddress &= ~(PAGE_SIZE - 1);
	Size = (Size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	
	uintptr_t Start = StartAddress;
	uintptr_t End   = StartAddress + Size;
	
	for (size_t i = 0; i < Lpb->MemoryRegionCount; i++)
	{
		PLOADER_MEMORY_REGION OtherRegion = &KiMemoryRegions[i];
		if (OtherRegion->Type != LOADER_MEM_FREE)
			continue;
		
		uintptr_t OrStart = OtherRegion->Base;
		uintptr_t OrEnd   = OtherRegion->Base + OtherRegion->Size;
		
		if (OrEnd <= Start || End <= OrStart)
		{
			// Not overlapping
			continue;
		}
		
		if (Start <= OrStart && OrEnd <= End)
		{
			// region completely swallowed, so nuke it
			OtherRegion->Type = LOADER_MEM_RESERVED;
			continue;
		}
		
		if (OrStart <= Start && End <= OrEnd)
		{
			// The region we are trying to erase is completely within
			// this memory region.
			
			// We need to create two memory regions:
			// OrStart -- Start - End -- OrEnd
			
			// First, check the trivial cases
			if (OrStart == Start)
			{
				// just set the start to the end
				OtherRegion->Base = End;
				OtherRegion->Size = OrEnd - End;
			}
			else if (OrEnd == End)
			{
				// just set the end to the start
				OtherRegion->Size = Start - OrStart;
			}
			else
			{
				// need to create a separate region
				OtherRegion->Size = Start - OrStart;
				
				if (Lpb->MemoryRegionCount >= MAX_MEMORY_REGIONS)
					continue;
				
				PLOADER_MEMORY_REGION NewRegion = &KiMemoryRegions[Lpb->MemoryRegionCount++];
				NewRegion->Type = LOADER_MEM_FREE;
				NewRegion->Base = End;
				NewRegion->Size = OrEnd - End;
			}
			
			continue;
		}
		
		if (OrStart < End && End < OrEnd && Start < OrStart)
			OrStart = End;
		
		if (Start < OrEnd && OrEnd < End && OrStart < Start)
			OrEnd = Start;
		
		if (OrEnd < OrStart)
		{
			OtherRegion->Type = LOADER_MEM_RESERVED;
			continue;
		}
		
		OtherRegion->Base = OrStart;
		OtherRegion->Size = OrEnd - OrStart;
	}
}

INIT
static void KiInitializeMemoryRegions()
{
	if (~KiMultibootInfo->flags & MULTIBOOT_INFO_MEM_MAP)
		KeCrashBeforeSMPInit("ERROR: There is no memory map specified!");
	
	PLOADER_PARAMETER_BLOCK Lpb = &KeLoaderParameterBlock;
	multiboot_memory_map_t
		*Mmap = P2V(KiMultibootInfo->mmap_addr),
		*MmapStart = Mmap,
		*MmapEnd = (void*)((uintptr_t)Mmap + KiMultibootInfo->mmap_length);
	
	// First, find all the available entries, and insert them.  During this loop,
	// any overlapping regions are resized/merged/erased.
	size_t Index = 0;
	for (; Mmap < MmapEnd; Mmap = (void*)((uintptr_t)Mmap + Mmap->size + sizeof(Mmap->size)))
	{
		if (Mmap->type != MULTIBOOT_MEMORY_AVAILABLE)
			continue;
		
		// ignore ranges that start into 64-bit memory
		if (Mmap->addr > 0x100000000)
			continue;
		
		// cap ranges that start into 64-bit memory
		if (Mmap->addr + Mmap->len > 0x100000000)
			Mmap->len = 0x100000000 - Mmap->addr;
		
		PLOADER_MEMORY_REGION MemoryRegion = &KiMemoryRegions[Index];
		MemoryRegion->Base = Mmap->addr;
		MemoryRegion->Size = Mmap->len;
		
		// Ensure the address and length are page size aligned.
		uint32_t Bias = PAGE_SIZE - (MemoryRegion->Base & (PAGE_SIZE - 1));
		if (Bias != PAGE_SIZE)
		{
			MemoryRegion->Size -= Bias;
			MemoryRegion->Base += Bias; // addr is now aligned
		}
		
		MemoryRegion->Size = MemoryRegion->Size & ~(PAGE_SIZE - 1);
		if (MemoryRegion->Size == 0)
			continue;
		
		MemoryRegion->Type = LOADER_MEM_FREE;
		
		// Ensure this range doesn't overlap with anything else.
		uintptr_t Start = MemoryRegion->Base;
		uintptr_t End   = MemoryRegion->Base + MemoryRegion->Size;
		
		for (size_t i = 0; i < Index; i++)
		{
			PLOADER_MEMORY_REGION OtherRegion = &KiMemoryRegions[i];
			
			uintptr_t OrStart = OtherRegion->Base;
			uintptr_t OrEnd   = OtherRegion->Base + OtherRegion->Size;
			
			if (OrStart <= Start && End <= OrEnd)
			{
				// new region completely inside old, discard.
				Start = End = 0;
				break;
			}
			
			if (Start <= OrStart && OrEnd <= End)
			{
				// the new segment completely swallows the old one.
				OtherRegion->Base = Start;
				OtherRegion->Size = End - Start;
				Start = End = 0;
				break;
			}
			
			if (OrStart < End && Start <= OrStart && End <= OrEnd)
				End = OrStart;
			
			if (Start < OrEnd && OrStart <= Start && OrEnd <= End)
				Start = OrEnd;
		}
		
		MemoryRegion->Base = Start;
		
		if (End < Start)
			MemoryRegion->Size = 0;
		else
			MemoryRegion->Size = End - Start;
		
		if (MemoryRegion->Size != 0)
			Index++;
		else
			MemoryRegion->Type = LOADER_MEM_RESERVED;
		
		if (Index >= MAX_MEMORY_REGIONS)
		{
			DbgPrint(
				"BOOT WARNING: The bootloader provided %zu bytes of entries, but we have a maximum of %d "
				"entries, and as such, some of the memory will be invisible to the OS.",
				KiMultibootInfo->mmap_length,
				MAX_MEMORY_REGIONS
			);
		}
	}
	
	Lpb->MemoryRegionCount = Index;
	Lpb->MemoryRegions = KiMemoryRegions;
	
	Mmap = MmapStart;
	for (; Mmap < MmapEnd; Mmap = (void*)((uintptr_t)Mmap + Mmap->size + sizeof(Mmap->size)))
	{
		if (Mmap->type == MULTIBOOT_MEMORY_AVAILABLE)
			continue;
		
		KiRemoveAreaFromMemMap(Mmap->addr, Mmap->len);
	}
}

extern char KiKernelEnd[];

static LOADER_AP KiLoaderAp;
static LOADER_FRAMEBUFFER KiLoaderFramebuffer;
static void* KiLoaderApDummy;

INIT
void KiInitLoaderParameterBlock()
{
	// Initialize the base identity mapping.
	MiInitializeBaseIdentityMapping();
	PLOADER_PARAMETER_BLOCK Lpb = &KeLoaderParameterBlock;
	
	KiMultibootInfo = P2V(KiMultibootInfo);
	
	if (KiMultibootSignature != MULTIBOOT_BOOTLOADER_MAGIC)
		KeCrashBeforeSMPInit("KiMultibootSignature is not %08x, it's %08x!", MULTIBOOT_BOOTLOADER_MAGIC, KiMultibootSignature);
	
	// Initialize the memory regions.
	KiInitializeMemoryRegions();
	KiRemoveAreaFromMemMap(0x100000, (size_t) KiKernelEnd - 0xC0100000);
	
	// Initialize the kernel module.
	Lpb->ModuleInfo.Kernel.Path   = "kernel.elf";
	Lpb->ModuleInfo.Kernel.String = P2V(KiMultibootInfo->cmdline);
	Lpb->ModuleInfo.Kernel.Address = (void*) 0xC0100000; // TODO: is the whole kernel (+ELF stuff) loaded here??
	Lpb->ModuleInfo.Kernel.Size    = (size_t) KiKernelEnd - 0xC0100000;
	
	// Initialize the other modules.
	if (KiMultibootInfo->flags & MULTIBOOT_INFO_MODS)
	{
		Lpb->ModuleInfo.Count = KiMultibootInfo->mods_count;
		Lpb->ModuleInfo.List  = KiEarlyAllocateMemoryFromMemMap(Lpb->ModuleInfo.Count * sizeof(LOADER_MODULE));
		
		// QUIRK: Multiboot1 does *not* give you the name of modules!
		// So their name has to be specified through the commandline.
		// Wow, that sucks. Also Multiboot2 doesn't either.
		multiboot_module_t* Module = P2V(KiMultibootInfo->mods_addr);
		KiRemoveAreaFromMemMap(KiMultibootInfo->mods_addr, Lpb->ModuleInfo.Count * sizeof(multiboot_module_t));

		for (size_t i = 0; i < Lpb->ModuleInfo.Count; i++)
		{
			PLOADER_MODULE Mod = &Lpb->ModuleInfo.List[i];
			Mod->Address = P2V(Module->mod_start);
			Mod->Size    = Module->mod_end - Module->mod_start;
			Mod->String  = "";
			
			if (Module->cmdline == 0)
				KeCrashBeforeSMPInit("ERROR: Cannot load module table.  This module has no name.");
			
			Mod->Path = P2V(Module->cmdline);
			
			KiRemoveAreaFromMemMap(Module->mod_start, Module->mod_end - Module->mod_start);
			Module++;
		}
	}
	else
	{
		DbgPrint("Booted without modules, the system WILL NOT boot!");
		Lpb->ModuleInfo.Count = 0;
		Lpb->ModuleInfo.List = NULL;
	}
	
	// Initialize the CPUs.
	Lpb->Multiprocessor.Count = 1;
	Lpb->Multiprocessor.List = &KiLoaderAp;
	Lpb->Multiprocessor.BootstrapHardwareId = 0;
	
	KiLoaderAp.ProcessorId = 0;
	KiLoaderAp.HardwareId = 0;
	KiLoaderAp.TrampolineJumpAddress = &KiLoaderApDummy;
	KiLoaderAp.ExtraArgument = NULL;
	
	// Initialize the frame buffers.
	if (KiMultibootInfo->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)
	{
		Lpb->FramebufferCount = 1;
		Lpb->Framebuffers = &KiLoaderFramebuffer;
		
		PLOADER_FRAMEBUFFER Fb = &KiLoaderFramebuffer;
		if (KiMultibootInfo->framebuffer_addr > 0x100000000)
		{
			KeCrash(
				"KiMultibootInfo->framebuffer_addr is %08x%08x, which is larger than 32-bit!",
				(uint32_t)KiMultibootInfo->framebuffer_addr,
				(uint32_t)(KiMultibootInfo->framebuffer_addr >> 32)
			);
		}
		
		Fb->Address  = (void*) (uint32_t) KiMultibootInfo->framebuffer_addr;
		Fb->Pitch    = KiMultibootInfo->framebuffer_pitch;
		Fb->Width    = KiMultibootInfo->framebuffer_width;
		Fb->Height   = KiMultibootInfo->framebuffer_height;
		Fb->BitDepth = KiMultibootInfo->framebuffer_bpp;
		
		// @BROKEN: Limine <v11.x (so, basically, every Limine version up to 17/10/2025, as
		// v11.x isn't released yet), places color information starting at offset 110 within
		// the multiboot_info_t struct.  However, GRUB places it at the offset mentioned in
		// the specification's own struct definition.
		//
		// Which one's wrong? Theoretically, the specification conflicts itself. In practice,
		// the canonical implementation of the multiboot protocol (GRUB) defines the struct
		// in the way the C definition of struct multiboot_info (in the specification) does.
		// As such, there is no way to win. Sorry.
		
		Fb->RedMaskSize     = KiMultibootInfo->u2.framebuffer_red_mask_size;
		Fb->RedMaskShift    = KiMultibootInfo->u2.framebuffer_red_field_position;
		Fb->GreenMaskSize   = KiMultibootInfo->u2.framebuffer_green_mask_size;
		Fb->GreenMaskShift  = KiMultibootInfo->u2.framebuffer_green_field_position;
		Fb->BlueMaskSize    = KiMultibootInfo->u2.framebuffer_blue_mask_size;
		Fb->BlueMaskShift   = KiMultibootInfo->u2.framebuffer_blue_field_position;
		
		// Quirk detection: If the values make no sense, believe this is Limine v10.x (or
		// earlier) loading us.
		if (Fb->RedMaskShift == Fb->GreenMaskShift ||
			Fb->RedMaskShift == Fb->BlueMaskShift ||
			Fb->GreenMaskShift == Fb->BlueMaskShift ||
			!Fb->RedMaskSize ||
			!Fb->GreenMaskSize ||
			!Fb->BlueMaskShift)
		{
			DbgPrint("Limine v10.x (or earlier) booted us, so correcting color information");
			Fb->RedMaskSize     = KiMultibootInfo->framebuffer_colorinfo_b;
			Fb->RedMaskShift    = KiMultibootInfo->framebuffer_colorinfo_a;
			Fb->GreenMaskSize   = KiMultibootInfo->u2.framebuffer_red_mask_size;
			Fb->GreenMaskShift  = KiMultibootInfo->u2.framebuffer_red_field_position;
			Fb->BlueMaskSize    = KiMultibootInfo->u2.framebuffer_green_mask_size;
			Fb->BlueMaskShift   = KiMultibootInfo->u2.framebuffer_green_field_position;
		}
	}
	else
	{
		DbgPrint("Booted without a framebuffer, things might go wrong!");
		Lpb->FramebufferCount = 0;
		Lpb->Framebuffers = NULL;
	}
	
	// Initialize the bootloader's information as well as the command line.
	Lpb->CommandLine = P2V(KiMultibootInfo->cmdline);
	
	if (KiMultibootInfo->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME)
	{
		Lpb->LoaderInfo.Name    = P2V(KiMultibootInfo->boot_loader_name);
		Lpb->LoaderInfo.Version = "v1.0";
	}
	else
	{
		Lpb->LoaderInfo.Name    = "Generic Multiboot1 compliant bootloader";
		Lpb->LoaderInfo.Version = "v1.0";
	}
}
