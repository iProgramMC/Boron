/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	sysinfo.h
	
Abstract:
	This header file contains the publicly exposed structure
	definitions for the OSQuerySystemInformation system service.
	
	Existing members of the struct will not be modified, and new
	members will only be added after existing members, in new
	revisions of the API.  User programs should generally increment
	constantly by the Size member, instead of relying on the size of
	the struct not changing.  Libboron.so may get away with directly
	indexing, because it's closely tied with the kernel (meaning,
	using an old version of libboron.so with a newer kernel already
	does not work).
	
Author:
	iProgramInCpp - 4 March 2026
***/
#pragma once

#define OS_NAME_SIZE (16)

typedef struct
{
	short Size;
	
	uint32_t OSVersionNumber;
	char OSName[OS_NAME_SIZE];
	
	uint32_t PageSize;
	uint32_t ProcessorCount;
	uint32_t AllocationGranularity;
	uintptr_t MinimumUserModeAddress;
	uintptr_t MaximumUserModeAddress;
}
SYSTEM_BASIC_INFORMATION, *PSYSTEM_BASIC_INFORMATION;

typedef struct
{
	short Size;
	
	int Status;
	
	uint32_t ParentProcessId;
}
SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

// Currently, ex/process.h defines MAX_IMAGE_NAME = 32.  Clone this definition so that incrementing
// the internal capacity of the image name does not affect the layout of the process structure.
#define SPI_MAX_IMAGE_NAME (32)

typedef struct
{
	short Size;
	
	// Information pertaining to the process.
	HANDLE ProcessId;
	
	// Information pertaining to the main thread, which is the first thread in the thread list.
	SYSTEM_THREAD_INFORMATION MainThread;
	
	// TODO: add more members here
	
	char ImageName[SPI_MAX_IMAGE_NAME];
}
SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

typedef struct
{
	short Size;
	
	uint32_t PageSize;
	
	size_t TotalPhysicalMemoryPages;
	size_t FreePhysicalMemoryPages;
	
	// TODO: add more members here.
}
SYSTEM_MEMORY_INFORMATION, *PSYSTEM_MEMORY_INFORMATION;




#define NEXT_SYSTEM_INFORMATION(Ptr) ((void*)((uintptr_t)(Ptr) + (Ptr)->Size))

// List of possible system information classes.
enum
{
	QUERY_BASIC_INFORMATION,
	QUERY_MEMORY_INFORMATION,
	QUERY_PROCESS_INFORMATION,
	QUERY_THREAD_INFORMATION,
	QUERY_MAXIMUM
};
