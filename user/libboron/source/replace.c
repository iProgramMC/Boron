/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	libboron/source/replace.c
	
Abstract:
	This module implements the OSReplaceProcess function.
	
	This function replaces the current process' executable with another
	entirely different executable, however it does so without creating a
	new process.
	
	This system function can be used to implement the POSIX "execve()"
	service, as it is similar in functionality.
	
	Note however that due to the way Boron is structured, some failure
	modes will lead to a process crash/abort instead of returning to the
	original process with an error code.  For example, if other threads
	are consuming memory, and between the time the system has finished
	unmapping every segment regarding the original process and the time
	libboron tries to map the new process, all resources are exhausted,
	the process will simply exit with a status code.
	
	Also, any extant thread other than the thread performing the replace-
	ment will likely crash the entire process.  So it is not recommended
	to leave auxiliary threads running in the system.
	
Author:
	iProgramInCpp - 4 January 2026
***/
#include <boron.h>
#include <rtl/elf.h>
#include "pebteb.h"
#include "dll.h"

// TODO: Not sure this is supported by all linkers?
extern char _end[];

typedef struct {
	uintptr_t Start;
	uintptr_t End;
}
MEMORY_RANGE, *PMEMORY_RANGE;

#define SWAP(mra, mrb) do { \
	MEMORY_RANGE tmp; \
	tmp = *(mra);     \
	*(mra) = *(mrb);  \
	*(mrb) = tmp;     \
} while (0)

static void OSDLLSortMemoryRanges(PMEMORY_RANGE MemoryRanges, size_t Count)
{
	// A selection sort should do the trick.
	for (size_t i = 0; i < Count; i++)
	{
		for (size_t j = i + 1; j < Count; j++)
		{
			if (MemoryRanges[i].Start < MemoryRanges[j].Start)
				continue;
			
			SWAP(&MemoryRanges[i], &MemoryRanges[j]);
		}
	}
}

static void OSDLLFixUpMemoryRanges(PMEMORY_RANGE MemoryRanges, size_t Count)
{
	for (size_t i = 0; i < Count; i++)
	{
		MemoryRanges[i].Start &= ~(PAGE_SIZE - 1);
		MemoryRanges[i].End = PAGE_ALIGN_UP(MemoryRanges[i].End);
	}
}

BSTATUS OSReplaceProcess(const char* ImageName, const char* CommandLine, const char* Environment)
{
#define RANGE_COUNT 3
	MEMORY_RANGE MemoryRanges[RANGE_COUNT];
	
	// We'll be building up a list of ranges to exclude from the nuclear option, and
	// then applying it to the rest of the mappings using a partial OSFreeVirtualMemory.
	
	// The first range to exclude is the libboron image.
	MemoryRanges[0].Start = RtlGetImageBase();
	MemoryRanges[0].End = (uintptr_t) _end;
	
	void*  OldInterpreterBase = (void*) MemoryRanges[0].Start;
	size_t OldInterpreterSize = PAGE_ALIGN_UP(MemoryRanges[0].End - MemoryRanges[0].Start);
	
	// The second range to exclude is the PEB of the current process.
	PPEB Peb = OSGetCurrentPeb();
	MemoryRanges[1].Start = (uintptr_t) Peb;
	MemoryRanges[1].End = MemoryRanges[1].Start + Peb->PebFreeSize;
	
	// The final range to exclude is this thread's current stack.
	// Use OSQueryVirtualMemoryInformation to get the range covered by the stack.
	VIRTUAL_MEMORY_INFORMATION StackInformation;
	BSTATUS Status = OSQueryVirtualMemoryInformation(
		CURRENT_PROCESS_HANDLE,
		&StackInformation,
		(uintptr_t) &StackInformation
	);
	
	if (FAILED(Status))
	{
		DbgPrint(
			"OSDLL: OSReplaceProcess failed to get info about current thread's stack: %s (%d)",
			RtlGetStatusString(Status),
			Status
		);
		return Status;
	}
	
	MemoryRanges[2].Start = StackInformation.Start;
	MemoryRanges[2].End = StackInformation.Start + StackInformation.Size;
	
	OSDLLSortMemoryRanges(MemoryRanges, RANGE_COUNT);
	OSDLLFixUpMemoryRanges(MemoryRanges, RANGE_COUNT);
	
	bool UnmappedAnything = false;
	
	uintptr_t LastStart = 0;
	
	for (int i = 0; i < RANGE_COUNT + 1; i++)
	{
		uintptr_t ThisStart = i == RANGE_COUNT ? USER_SPACE_END : MemoryRanges[i].Start;
		size_t Size = ThisStart - LastStart;
		
		if (Size != 0)
		{
			Status = OSFreeVirtualMemory(
				CURRENT_PROCESS_HANDLE,
				(void*) LastStart,
				Size,
				MEM_RELEASE | MEM_PARTIAL
			);
		}
		
		if (FAILED(Status))
		{
			DbgPrint(
				"OSDLL: OSReplaceProcess failed to unmap beginning of address space: %s (%d)",
				RtlGetStatusString(Status),
				Status
			);
			
			if (UnmappedAnything)
			{
				// TODO: report an error, and only then exit!
				OSExitProcess(1);
			}
			
			return Status;
		}
		
		UnmappedAnything = true;
		
		if (i < RANGE_COUNT) {
			LastStart = MemoryRanges[i].End;
		}
	}
	
	Peb->Loader.OldInterpreterBase = OldInterpreterBase;
	Peb->Loader.OldInterpreterSize = OldInterpreterSize;
	
	OSPrintf("Okay, now everything regarding this process should've been unmapped.\n");
	OSPrintf("Just rest for now.\n");
	OSSleep(99999999);
	return STATUS_UNIMPLEMENTED;
}
