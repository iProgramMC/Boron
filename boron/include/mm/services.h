/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/services.h
	
Abstract:
	This header defines the Boron memory manager's
	exposed system services.
	
Author:
	iProgramInCpp - 13 March 2025
***/
#pragma once

#include <status.h>

BSTATUS OSAllocateVirtualMemory(
	HANDLE ProcessHandle,
	void** BaseAddressInOut,
	size_t* RegionSizeInOut,
	int AllocationType,
	int Protection
);

BSTATUS OSFreeVirtualMemory(
	HANDLE ProcessHandle,
	void* BaseAddress,
	size_t RegionSize,
	int FreeType
);

BSTATUS OSMapViewOfObject(
	HANDLE ProcessHandle,
	HANDLE MappedObject,
	void** BaseAddressInOut,
	size_t ViewSize,
	int AllocationType,
	uint64_t SectionOffset,
	int Protection
);

BSTATUS OSWriteVirtualMemory(
	HANDLE ProcessHandle,
	void* TargetAddress,
	const void* Source,
	size_t ByteCount
);

BSTATUS OSReadVirtualMemory(
	HANDLE ProcessHandle,
	void* TargetAddress,
	const void* SourceAddress,
	size_t ByteCount
);

BSTATUS OSGetMappedFileHandle(
	PHANDLE OutHandle,
	HANDLE ProcessHandle,
	uintptr_t Address
);
