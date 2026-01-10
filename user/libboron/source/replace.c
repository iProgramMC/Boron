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
#include <string.h>
#include <rtl/assert.h>
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

// NOTE: mlibc will probably have its own copy instead of using libboron's version.
//
// This is so that it can keep features such as non-CLOEXEC file handles across the
// process replacement.  However, we might also add a list of memory ranges to not
// annihilate. We already do something like this internally.
BSTATUS OSReplaceProcess(const char* ImageName, const char* CommandLine, const char* Environment)
{
	BSTATUS Status;
	HANDLE FileHandle;
	
	PPEB Peb = OSGetCurrentPeb();
	
	if (!Environment) {
		Environment = Peb->Environment;
	}
	
	// Use OSQueryVirtualMemoryInformation to get the range covered by the stack.
	VIRTUAL_MEMORY_INFORMATION StackInformation;
	Status = OSQueryVirtualMemoryInformation(
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
	
	DbgPrint("StackInformation   Start: %p   Size: %zu", StackInformation.Start, StackInformation.Size);
	
	// Perform some sanity checks first.
	Status = OSDLLOpenFileByName(&FileHandle, ImageName, false);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Failed to open %s. %s (%d)", ImageName, RtlGetStatusString(Status), Status);
		return Status;
	}
	
	// First, we allocate a PEB for this new process.
	PPEB NewPeb = NULL;
	size_t NewPebSize = 0;
	Status = OSDLLCreatePebForProcess(&NewPeb, &NewPebSize, ImageName, CommandLine, Environment);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Failed to create PEB for process. %s (%d)", RtlGetStatusString(Status), Status);
		OSClose(FileHandle);
		return Status;
	}
	
	// Inherit the current directory.
	NewPeb->StartingDirectory = OSGetCurrentDirectory();
	
	// Then allocate an entirely new memory range so that we can exclude it from our nuclear
	// unmap operations later.
	void* PebPtr = NULL;
	size_t RegionSize = NewPebSize;
	Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&PebPtr,
		&RegionSize,
		MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN,
		PAGE_READ | PAGE_WRITE
	);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Cannot map PEB: %s (%d)", RtlGetStatusString(Status), Status);
		OSClose(FileHandle);
		OSFree(NewPeb);
		return Status;
	}
	
	// Relocate the pointers.
	NewPeb->ImageName   = (char*)((uintptr_t)NewPeb->ImageName   + (uintptr_t)PebPtr - (uintptr_t)NewPeb);
	NewPeb->CommandLine = (char*)((uintptr_t)NewPeb->CommandLine + (uintptr_t)PebPtr - (uintptr_t)NewPeb);
	NewPeb->Environment = (char*)((uintptr_t)NewPeb->Environment + (uintptr_t)PebPtr - (uintptr_t)NewPeb);
	NewPeb->PebFreeSize = RegionSize;
	
	// Copy the current PEB's standard I/O handles.
	for (int i = 0; i < 3; i++) {
		NewPeb->StandardIO[i] = Peb->StandardIO[i];
	}
	
	memcpy(PebPtr, NewPeb, NewPebSize);
	OSFree(NewPeb);
	
	OSSetCurrentPeb(PebPtr);
	Peb = (PPEB) PebPtr;
	
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
	MemoryRanges[1].Start = (uintptr_t) Peb;
	MemoryRanges[1].End = MemoryRanges[1].Start + Peb->PebFreeSize;
	
	// The final range to exclude is this thread's current stack.
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
	
	// Temporarily reinitialize the heap, but we'll free everything unconditionally later,
	// so don't pass any pointers from the heap to the new process!
	OSDLLReinitializeHeap();
	
	// Sadly we'll have to allocate a new TEB in order for the current directory to be preserved.
	Status = OSDLLCreateTeb(Peb, Peb->StartingDirectory);
	if (FAILED(Status))
	{
		DbgPrint("OSReplaceProcess: Failed to allocate temporary TEB. %s (%d)", Peb->ImageName, RtlGetStatusString(Status), Status);
		OSExitProcess(1);
	}
	
	OSDLL_ENTRY_POINT EntryPoint = NULL;
	
	// Now map the main image inside.
	LdrDbgPrint("OSReplaceProcess: Mapping main image %s.", Peb->ImageName);
	Status = OSDLLMapElfFile(Peb, CURRENT_PROCESS_HANDLE, FileHandle, Peb->ImageName, &EntryPoint, FILE_KIND_MAIN_EXECUTABLE, true);
	OSClose(FileHandle);
	
	if (FAILED(Status))
	{
		DbgPrint("OSReplaceProcess: Failed to map the %s executable. %s (%d)", Peb->ImageName, RtlGetStatusString(Status), Status);
		OSExitProcess(1);
	}
	
	ASSERT(Peb->Loader.MappedImage);
	
	if (Peb->Loader.Interpreter)
	{
		// There is an interpreter.
		const char* Interpreter = Peb->Loader.Interpreter;
		HANDLE InterpreterFileHandle = HANDLE_NONE;
		if (strcmp(Interpreter, "libboron.so") == 0)
		{
			LdrDbgPrint("OSReplaceProcess: The executable demands libboron.so, so map that.");
			
			// This is libboron.so, so no meaning in scanning for anything
			// because we can just open ourselves.
			Status = OSDLLOpenSelf(&InterpreterFileHandle);
		}
		else
		{
			LdrDbgPrint("OSReplaceProcess: Opening interpreter %s.", Interpreter);
			Status = OSDLLOpenFileByName(&InterpreterFileHandle, Interpreter, true);
		}
		
		LdrDbgPrint("OSReplaceProcess: Mapping interpreter %s.", Interpreter);
		
		if (FAILED(Status))
		{
			DbgPrint("OSReplaceProcess: Failed to map interpreter %s. %s (%d)", Interpreter, RtlGetStatusString(Status), Status);
			OSExitProcess(1);
		}
		
		Status = OSDLLMapElfFile(Peb, CURRENT_PROCESS_HANDLE, InterpreterFileHandle, Interpreter, &EntryPoint, FILE_KIND_INTERPRETER, true);
		OSClose(InterpreterFileHandle);
		
		if (FAILED(Status))
		{
			DbgPrint("OSReplaceProcess: Failed to map the interpreter %s. %s (%d)", Interpreter, RtlGetStatusString(Status), Status);
			OSExitProcess(1);
		}
	}
	
	OSDLLClearEntireHeap(NULL);
	
	// Finally, some assembly to jump to the new process' entry point.
	LdrDbgPrint("OSReplaceProcess: Entry Point: %p", EntryPoint);
	
	uintptr_t StackBottom = StackInformation.Start + StackInformation.Size;	
	OSDLLJumpToEntry(StackBottom, EntryPoint, PebPtr);
}
