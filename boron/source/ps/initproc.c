/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/initproc.c
	
Abstract:
	This module implements the code which initializes the
	initial user process in the Boron operating system.
	
	This initial process is implemented within Libboron.so.
	Its only job is to load the *actual* initialization
	process which is init.exe.
	
	In comparison
	
Author:
	iProgramInCpp - 16 April 2025
***/
#include "psp.h"
#include <io.h>
#include <rtl/elf.h>

#define USER_STACK_SIZE (256 * 1024)

static const char* PspInitialProcessFileName = "\\InitRoot\\libboron.so";

// TODO: Share a lot of this code with Ldr.

static PELF_PROGRAM_HEADER PspLdrFindDynamicPhdr(
	uint8_t* ProgramHeaders,
	size_t ProgramHeaderSize,
	size_t ProgramHeaderCount)
{
	for (size_t i = 0; i < ProgramHeaderCount; i++)
	{
		PELF_PROGRAM_HEADER ProgramHeader = (void*) (ProgramHeaders + i * ProgramHeaderSize);
		
		if (ProgramHeader->Type == PROG_DYNAMIC)
			return ProgramHeader;
	}
	
	return NULL;
}

NO_RETURN
void PspUserThreadStart(void* Context)
{
	// Context means the initial RIP in user mode.
	
	// First, allocate the stack.
	void* StackAddress = NULL;
	size_t StackSize = USER_STACK_SIZE;
	BSTATUS Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&StackAddress,
		&StackSize,
		MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN,
		PAGE_READ | PAGE_WRITE
	);
	
	ASSERT(SUCCEEDED(Status) && "TODO: What happens if this fails? Maybe should have set it up earlier?!");
	
	KeDescendIntoUserMode(Context, StackAddress);
}

NO_RETURN
void PsStartInitialProcess(UNUSED void* Context)
{
	BSTATUS Status;
	HANDLE ProcessHandle;
	HANDLE FileHandle;
	OBJECT_ATTRIBUTES FileAttributes;
	ELF_HEADER ElfHeader;
	const char* Func;
	
	Func = __func__;
	
	Status = OSCreateProcess(
		&ProcessHandle,
		NULL,
		CURRENT_PROCESS_HANDLE,
		false
	);
	
	if (FAILED(Status))
		KeCrash("%s: Failed to create initial process: %d (%s)", Status, RtlGetStatusString(Status));
	
	FileAttributes.RootDirectory = HANDLE_NONE;
	FileAttributes.ObjectName = PspInitialProcessFileName;
	FileAttributes.ObjectNameLength = strlen(PspInitialProcessFileName);
	FileAttributes.OpenFlags = 0;
	
	Status = OSOpenFile(
		&FileHandle,
		&FileAttributes
	);
	
	if (FAILED(Status))
		KeCrash("%s: Failed to open %s: %d (%s)", Func, PspInitialProcessFileName, Status, RtlGetStatusString(Status));
	
	// The file has been opened.
	uint64_t FileSize = 0;
	Status = OSGetLengthFile(FileHandle, &FileSize);
	if (FAILED(Status))
		KeCrash("%s: Failed to get length of file %s: %d (%s)", Func, PspInitialProcessFileName, Status, RtlGetStatusString(Status));
	
	// XXX: Arbitrary
	if (FileSize > 0x800000)
		KeCrash("%s: %s is just too big", Func, PspInitialProcessFileName);
	
	// There is no need to complicate ourselves at all.  We will simply map the
	// entire file once, then map each page individually where needed, and unmap
	// the temporary mapping.
	uint8_t* ElfMappingBase = NULL;
	size_t RegionSize = FileSize;
	
	Status = OSMapViewOfObject(
		CURRENT_PROCESS_HANDLE,
		FileHandle,
		(void**) &ElfMappingBase,
		RegionSize,
		0,
		0,
		PAGE_READ
	);
	
	if (FAILED(Status))
		KeCrash("%s: Could not map %s into memory: %d (%s)", Func, PspInitialProcessFileName, Status, RtlGetStatusString(Status));
	
	DbgPrint("Mapped entire file to %p", ElfMappingBase);
	
	PELF_HEADER PElfHeader = (PELF_HEADER) ElfMappingBase;
	memcpy(&ElfHeader, PElfHeader, sizeof(ELF_HEADER));
	
	// TODO: Check if this is a valid ELF header.
	
	// Read program header information.
	uint8_t* ProgramHeaders = ElfMappingBase + ElfHeader.ProgramHeadersOffset;
	
	uintptr_t BoronDllBase = 0;
	
	uintptr_t FirstAddr, LargestAddr;
	FirstAddr = (uintptr_t) ~0ULL;
	LargestAddr = 0;
	
	// Find the dynamic program header;
	PELF_PROGRAM_HEADER DynamicPhdr = PspLdrFindDynamicPhdr(
		ProgramHeaders,
		ElfHeader.ProgramHeaderSize,
		ElfHeader.ProgramHeaderCount
	);
	
	if (!DynamicPhdr)
		KeCrash("%s: Error, cannot find dynamic program header in libboron.so", Func);
	
	// Now map each of them in.
	for (int i = 0; i < ElfHeader.ProgramHeaderCount; i++)
	{
		PELF_PROGRAM_HEADER ProgramHeader = (void*) (ProgramHeaders + i * ElfHeader.ProgramHeaderSize);
		
		if (FirstAddr > ProgramHeader->VirtualAddress)
			FirstAddr = ProgramHeader->VirtualAddress;
		if (LargestAddr < ProgramHeader->VirtualAddress + ProgramHeader->SizeInMemory)
			LargestAddr = ProgramHeader->VirtualAddress + ProgramHeader->SizeInMemory;
	}
	
	if (FirstAddr > LargestAddr)
		KeCrash("%s: Error, first address %p > largest address %p", Func, FirstAddr, LargestAddr);
	
	uintptr_t Size = LargestAddr - FirstAddr;
	Size = (Size + PAGE_SIZE - 1) & (PAGE_SIZE - 1);
	
	// Now, the PEB will occupy however many pages, so we will need to cut those out too.
	size_t PebSize = (sizeof(PEB) + PAGE_SIZE - 1) & (PAGE_SIZE - 1);
	
	BoronDllBase = MM_USER_SPACE_END + 1 - (PebSize + Size) * PAGE_SIZE;
	DbgPrint("BoronDllBase: %p", BoronDllBase);
	
	bool IsDynamicLoaded = false;
	uintptr_t BoronDllBaseOld = BoronDllBase;
	uintptr_t DynamicBaseAddress = BoronDllBase + DynamicPhdr->VirtualAddress;
	
	// Start mapping in the program headers.
	for (int i = 0; i < ElfHeader.ProgramHeaderCount; i++)
	{
		PELF_PROGRAM_HEADER ProgramHeader = (void*) (ProgramHeaders + i * ElfHeader.ProgramHeaderSize);
		
		if (ProgramHeader->SizeInMemory == 0)
			continue;
		
		if (ProgramHeader->Type != PROG_LOAD)
			continue;
		
		void* BaseAddress = (void*)(BoronDllBase + ProgramHeader->VirtualAddress);
		
		if (BaseAddress == (void*) DynamicBaseAddress)
			IsDynamicLoaded = true;
		
		int Protection = 0, Flags = MEM_COW;
		if (ProgramHeader->Flags & ELF_PHDR_EXEC)  Protection |= PAGE_EXECUTE;
		if (ProgramHeader->Flags & ELF_PHDR_READ)  Protection |= PAGE_READ;
		
		BSTATUS Status = OSMapViewOfObject(
			ProcessHandle,
			FileHandle,
			&BaseAddress,
			ProgramHeader->SizeInMemory,
			Flags,
			ProgramHeader->Offset,
			Protection
		);

		if (FAILED(Status))
		{
			KeCrash(
				"%s: Failed to map ELF phdr in: %d (%s)\n"
				"ProgramHeader->SizeInMemory: %zu\n"
				"ProgramHeader->VirtualAddress: %p\n"
				"ProgramHeader->Offset: %p", Func,
				Status,
				RtlGetStatusString(Status),
				ProgramHeader->SizeInMemory,
				ProgramHeader->VirtualAddress,
				ProgramHeader->Offset
			);
		}
	}
	
	// Program headers have been mapped.
	BoronDllBase = BoronDllBaseOld;
	
	// This assumes that the dynamic table is entirely mapped.
	if (!IsDynamicLoaded)
		KeCrash("%s: There is no LOAD segment that maps the DYNAMIC table.", Func);
	
	// Attach to this process.
	PEPROCESS Process = NULL;
	Status = ExReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, (void**) &Process);
	ASSERT(SUCCEEDED(Status));
	
	// Before attaching to the process make sure to unmap from the system address space.
	Status = OSFreeVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		ElfMappingBase,
		RegionSize,
		MEM_RELEASE
	);
	if (FAILED(Status))
		KeCrash("%s: Failed to free initial mapping of ELF: %d (%s)\n%p %zu", Func, Status, RtlGetStatusString(Status), ElfMappingBase, RegionSize);
	
	PsAttachToProcess(Process);
	
	// Re-map it into the new process' address space so we have things easily accessible everywhere.
	uintptr_t OldBase = (uintptr_t) ElfMappingBase;
	
	ElfMappingBase = NULL;
	
	Status = OSMapViewOfObject(
		CURRENT_PROCESS_HANDLE,
		FileHandle,
		(void**) &ElfMappingBase,
		RegionSize,
		0,
		0,
		PAGE_READ
	);
	
	if (FAILED(Status))
		KeCrash("%s: Could not map %s into memory: %d (%s)", Func, PspInitialProcessFileName, Status, RtlGetStatusString(Status));
	
	// Some cool math to move the dynamic header into the new mapping.
	DynamicPhdr = (PELF_PROGRAM_HEADER) ((uintptr_t)DynamicPhdr - OldBase + (uintptr_t)ElfMappingBase);
	
	ELF_DYNAMIC_INFO DynInfo;
	PELF_DYNAMIC_ITEM DynTable = (PELF_DYNAMIC_ITEM) (BoronDllBase + DynamicPhdr->VirtualAddress);
	
	if (!RtlParseDynamicTable(DynTable, &DynInfo, BoronDllBase))
		KeCrash("%s: Failed to parse dynamic table.", Func);
	
	if (!RtlPerformRelocations(&DynInfo, BoronDllBase))
		KeCrash("%s: Failed to perform relocations.", Func);
	
	RtlParseInterestingSections(ElfMappingBase, &DynInfo, BoronDllBase);
	RtlUpdateGlobalOffsetTable(DynInfo.GlobalOffsetTable, DynInfo.GlobalOffsetTableSize, BoronDllBase);
	RtlUpdateGlobalOffsetTable(DynInfo.GotPlt, DynInfo.GotPltSize, BoronDllBase);
	
	if (!RtlLinkPlt(&DynInfo, BoronDllBase, false, PspInitialProcessFileName))
		KeCrash("%s: Failed to perform PLT linking.", Func);
	
	// TODO: Adjust permissions so that the code segment is not writable anymore even with CoW.
	
	// Unmap the temporary mapping of the ELF, only leaving the actual mappings in.
	Status = OSFreeVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		ElfMappingBase,
		RegionSize,
		MEM_RELEASE
	);
	ASSERT(SUCCEEDED(Status));
	
	PsDetachFromProcess();
	ObDereferenceObject(Process);
	OSClose(FileHandle);
	
	void* EntryPoint = (void*) (BoronDllBase + ElfHeader.EntryPoint);
	
	// OK. Now, the process is mapped.
	// Create a thread to make it call the entry point.
	HANDLE ThreadHandle;
	Status = PsCreateSystemThread(
		&ThreadHandle,
		NULL,
		ProcessHandle,
		PspUserThreadStart,
		EntryPoint
	);
	
	if (FAILED(Status))
		KeCrash("%s: Failed to create thread in process: %d (%s)", Func, Status, RtlGetStatusString(Status));
	
	OSClose(ThreadHandle);
	OSClose(ProcessHandle);
	PsTerminateThread();
}

INIT
bool PsInitSystemPart2()
{
	HANDLE ThreadHandle;
	BSTATUS Status;
	
	Status = PsCreateSystemThread(
		&ThreadHandle,
		NULL,
		HANDLE_NONE,
		PsStartInitialProcess,
		NULL
	);
	
	if (FAILED(Status))
		DbgPrint("Failed to setup initial process: %d", Status);
	
	return SUCCEEDED(Status);
}
