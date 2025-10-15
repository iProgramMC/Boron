#pragma once

#include <main.h>

typedef struct
{
	char* Path;
	char* String; // associated string a.k.a. cmdline
	void* Address;
	size_t Size;
}
LOADER_MODULE, *PLOADER_MODULE;

typedef struct
{
	const char* Name;
	const char* Version;
}
LOADER_INFO, *PLOADER_INFO;

typedef struct
{
	void*    Address;  // physical address of frame buffer (HHDM offset on 64-bit)
	uint32_t Pitch;    // width of a single row in bytes
	uint32_t Width;
	uint32_t Height;
	uint8_t  BitDepth;
}
LOADER_FRAMEBUFFER_INFO, PLOADER_FRAMEBUFFER_INFO;

// Application Processor
typedef struct
{
	uint32_t ProcessorId;
	uint32_t HardwareId;  // LAPIC ID on x86/x86_64
	void*    TrampolineJumpAddress;
	void*    ExtraArgument;
}
LOADER_AP, *PLOADER_AP;

// MultiProcessor Information
typedef struct
{
	uint32_t   BootstrapHardwareId;
	uint32_t   Count;
	PLOADER_AP List;
}
LOADER_MP_INFO, *PLOADER_MP_INFO;

enum
{
	LOADER_MEM_FREE,
	LOADER_MEM_BAD,
	LOADER_MEM_RESERVED,
	LOADER_MEM_ACPI_RECLAIMABLE,
	LOADER_MEM_LOADER_RECLAIMABLE,
	LOADER_MEM_LOADED_PROGRAM,
};

// NOTE: LOADER_MEM_FREE memory regions are GUARANTEED to be page aligned.
//
// NOTE: On i386, there may be regions above the 32-bit boundary, but they will
// be ignored, because we do not implement PAE.
//
// PAE: If we do implement PAE, change these to uint64_t for i386.
typedef struct
{
	uintptr_t Base;
	uintptr_t Size;
	uintptr_t Type;
}
LOADER_MEMORY_REGION, *PLOADER_MEMORY_REGION;

typedef struct
{
	PLOADER_MEMORY_REGION MemoryRegions;
	size_t MemoryRegionCount;
	
	LOADER_MODULE KernelModule;
	PLOADER_MODULE Modules;
	size_t ModuleCount;
	
	LOADER_MP_INFO Multiprocessor;
	
	LOADER_INFO LoaderInfo;
	
	const char* CommandLine;
	
#ifdef IS_64_BIT
	uintptr_t HhdmBase;
#endif

#if defined TARGET_AMD64 || defined TARGET_I386
	void* RsdpAddress; // physical address of RSDP table - required for ACPI support
	// note: Multiboot on i386 does *not* give us the pointer to the RSD PTR table,
	// so may have to scan manually
#endif

#if 0
	// device trees are unused for now but they can be used
	void* DeviceTreeBlobAddress;
#endif
}
LOADER_PARAMETER_BLOCK, *PLOADER_PARAMETER_BLOCK;
