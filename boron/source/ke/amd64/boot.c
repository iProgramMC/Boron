/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/amd64/boot.c
	
Abstract:
	This module contains the bootstrap code that converts
	Limine requests into kernel specific definitions.
	
Author:
	iProgramInCpp - 15 October 2025
***/
#include <ke.h>
#include <_limine.h>

#if LIMINE_API_REVISION >= 3
	#define STRING_OR_CMDLINE string
#else
	#define STRING_OR_CMDLINE cmdline
#endif

#if LIMINE_API_REVISION >= 2
	#define __LIMINE_MEMMAP_KERNEL_AND_MODULES LIMINE_MEMMAP_EXECUTABLE_AND_MODULES
#else
	#define __LIMINE_MEMMAP_KERNEL_AND_MODULES LIMINE_MEMMAP_KERNEL_AND_MODULES
#endif

// ======= Limine Requests =======

volatile struct limine_hhdm_request KeLimineHhdmRequest =
{
	.id       = LIMINE_HHDM_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_framebuffer_request KeLimineFramebufferRequest =
{
	.id = LIMINE_FRAMEBUFFER_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_memmap_request KeLimineMemMapRequest =
{
	.id       = LIMINE_MEMMAP_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_module_request KeLimineModuleRequest =
{
	.id       = LIMINE_MODULE_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_smp_request KeLimineSmpRequest =
{
	.id       = LIMINE_SMP_REQUEST,
	.revision = 0,
	.response = NULL,
	.flags    = 0,
};
volatile struct limine_rsdp_request KeLimineRsdpRequest =
{
	.id       = LIMINE_RSDP_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_kernel_file_request KeLimineKernelFileRequest =
{
	.id       = LIMINE_KERNEL_FILE_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_bootloader_info_request KeLimineBootloaderInfoRequest =
{
	.id       = LIMINE_BOOTLOADER_INFO_REQUEST,
	.revision = 0,
	.response = NULL,
};

// ======= Loader Parameter Block =======
LOADER_PARAMETER_BLOCK KeLoaderParameterBlock;
extern uintptr_t MmHHDMBase; // mm/pmm.c

// TODO: This is kind of a space waste.
#define MAX_MEMORY_REGIONS 256
static LOADER_MEMORY_REGION KiMemoryRegions[MAX_MEMORY_REGIONS];

INIT
static int KiConvertLimineMemoryTypeToRegionType(uint64_t Type)
{
	switch (Type)
	{
		case LIMINE_MEMMAP_USABLE:
			return LOADER_MEM_FREE;

		case LIMINE_MEMMAP_BAD_MEMORY:
			return LOADER_MEM_BAD;

		case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
			return LOADER_MEM_ACPI_RECLAIMABLE;

		case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
			return LOADER_MEM_LOADER_RECLAIMABLE;

		case __LIMINE_MEMMAP_KERNEL_AND_MODULES:
			return LOADER_MEM_LOADED_PROGRAM;

		case LIMINE_MEMMAP_RESERVED:
		default:
			return LOADER_MEM_RESERVED;
	}
}

INIT
static void KiConvertLimineFileToLoaderModule(PLOADER_MODULE LoaderModule, struct limine_file* LimineFile)
{
	LoaderModule->Address = LimineFile->address;
	LoaderModule->Path    = LimineFile->path;
	LoaderModule->Size    = LimineFile->size;
	LoaderModule->String  = LimineFile->STRING_OR_CMDLINE;
}

INIT
static void KiConvertLimineApToLoaderAp(PLOADER_AP LoaderAp, struct limine_smp_info* LimineAp)
{
	LoaderAp->ProcessorId   = LimineAp->processor_id;
	LoaderAp->HardwareId    = LimineAp->lapic_id;
	LoaderAp->ExtraArgument = 0;
	LoaderAp->TrampolineJumpAddress = LimineAp->goto_address;
}

// Allocate a contiguous area of memory from the memory map.
INIT
static void* KiLimineAllocateMemoryFromMemMap(size_t Size)
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
	
	KeCrashBeforeSMPInit("Error, out of memory in KiLimineAllocateMemoryFromMemMap");
}

INIT
void KiInitLoaderParameterBlock()
{
	PLOADER_PARAMETER_BLOCK Lpb = &KeLoaderParameterBlock;
	
	// Crash if certain responses haven't been provided.
#define CHECK_RESPONSE(n) do { \
	if (!(n).response) \
		KeCrashBeforeSMPInit("BOOT_ERROR: " #n " was not provided"); \
} while (0)
	
	CHECK_RESPONSE(KeLimineFramebufferRequest);
	CHECK_RESPONSE(KeLimineHhdmRequest);
	CHECK_RESPONSE(KeLimineMemMapRequest);
	CHECK_RESPONSE(KeLimineSmpRequest);
	CHECK_RESPONSE(KeLimineRsdpRequest);
	CHECK_RESPONSE(KeLimineModuleRequest);
	CHECK_RESPONSE(KeLimineKernelFileRequest);
	CHECK_RESPONSE(KeLimineBootloaderInfoRequest);
	
	// Initialize the memory regions.
	struct limine_memmap_response* MemMapResponse = KeLimineMemMapRequest.response;
	if (MemMapResponse->entry_count >= MAX_MEMORY_REGIONS)
	{
		DbgPrint(
			"BOOT WARNING: The bootloader provided %zu entries, bigger than our maximum of %d, "
			"some of the memory will be invisible to the OS.",
			MemMapResponse->entry_count,
			MAX_MEMORY_REGIONS
		);
		
		MemMapResponse->entry_count = MAX_MEMORY_REGIONS;
	}
	
	for (size_t i = 0; i < MemMapResponse->entry_count; i++)
	{
		PLOADER_MEMORY_REGION Region = &KiMemoryRegions[i];
		struct limine_memmap_entry* Entry = MemMapResponse->entries[i];
		
		Region->Base = Entry->base;
		Region->Size = Entry->length;
		Region->Type = KiConvertLimineMemoryTypeToRegionType(Entry->type);
	}
	
	Lpb->MemoryRegions = KiMemoryRegions;
	Lpb->MemoryRegionCount = MemMapResponse->entry_count;
	
	// Initialize the HHDM base.
	Lpb->HhdmBase = MmHHDMBase = KeLimineHhdmRequest.response->offset;
	
	// Initialize the kernel module.
	KiConvertLimineFileToLoaderModule(&Lpb->KernelModule, KeLimineKernelFileRequest.response->kernel_file);
	
	// Initialize the other modules.
	Lpb->ModuleCount = KeLimineModuleRequest.response->module_count;
	Lpb->Modules = KiLimineAllocateMemoryFromMemMap(Lpb->ModuleCount * sizeof(LOADER_MODULE));
	
	for (size_t i = 0; i < Lpb->ModuleCount; i++)
		KiConvertLimineFileToLoaderModule(&Lpb->Modules[i], KeLimineModuleRequest.response->modules[i]);
	
	// Initialize the CPUs.
	Lpb->Multiprocessor.BootstrapHardwareId = KeLimineSmpRequest.response->bsp_lapic_id;
	Lpb->Multiprocessor.Count = KeLimineSmpRequest.response->cpu_count;
	Lpb->Multiprocessor.List = KiLimineAllocateMemoryFromMemMap(Lpb->Multiprocessor.Count * sizeof(LOADER_AP));
	
	for (size_t i = 0; i < Lpb->Multiprocessor.Count; i++)
		KiConvertLimineApToLoaderAp(&Lpb->Multiprocessor.List[i], KeLimineSmpRequest.response->cpus[i]);
	
	// Initialize the bootloader's information as well as the command line.
	Lpb->CommandLine        = KeLimineKernelFileRequest.response->kernel_file->STRING_OR_CMDLINE;
	Lpb->LoaderInfo.Name    = KeLimineBootloaderInfoRequest.response->name;
	Lpb->LoaderInfo.Version = KeLimineBootloaderInfoRequest.response->version;
	
	// Initialize the RsdpAddress.
	Lpb->RsdpAddress = KeLimineRsdpRequest.response->address;
}

