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
#include <elf.h>
#include <io.h>

static const char* PspInitialProcessFileName = "\\InitRoot\\libboron.so";

// TODO: Share a lot of this code with Ldr.

NO_RETURN
void PsStartInitialProcess(UNUSED void* Context)
{
	BSTATUS Status;
	HANDLE ProcessHandle;
	HANDLE FileHandle;
	OBJECT_ATTRIBUTES FileAttributes;
	IO_STATUS_BLOCK Iosb;
	ELF_HEADER ElfHeader;
	
	Status = OSCreateProcess(
		&ProcessHandle,
		NULL,
		CURRENT_PROCESS_HANDLE,
		false
	);
	
	if (FAILED(Status))
		KeCrash("Failed to create initial process: %d", Status);
	
	FileAttributes.RootDirectory = HANDLE_NONE;
	FileAttributes.ObjectName = PspInitialProcessFileName;
	FileAttributes.ObjectNameLength = strlen(PspInitialProcessFileName);
	FileAttributes.OpenFlags = 0;
	
	Status = OSOpenFile(
		&FileHandle,
		&FileAttributes
	);
	
	if (FAILED(Status))
		KeCrash("Failed to open %s: %d", PspInitialProcessFileName, Status);
	
	// The file has been opened.
	// Read the ELF header.
	Status = OSReadFile(&Iosb, FileHandle, &ElfHeader, sizeof(ELF_HEADER), 0);
	
	if (FAILED(Status) || Iosb.BytesRead < sizeof(ELF_HEADER))
		KeCrash("Failed to read ELF header: %d", Status);
	
	// TODO: Check if you are a valid ELF header.
	
	// Read program header information.
	size_t ProgramHeaderListSize = ElfHeader.ProgramHeaderSize * ElfHeader.ProgramHeaderCount;
	uint8_t* ProgramHeaders = MmAllocatePool(POOL_PAGED, ProgramHeaderListSize);
	if (!ProgramHeaders)
		KeCrash("Out of memory while trying to allocate program headers");
	
	Status = OSSeekFile(FileHandle, ElfHeader.ProgramHeadersOffset, IO_SEEK_SET);
	if (FAILED(Status))
		KeCrash("Failed to seek to program headers offset");
	
	Status = OSReadFile(&Iosb, FileHandle, ProgramHeaders, ProgramHeaderListSize, 0);
	if (FAILED(Status) || Iosb.BytesRead < ProgramHeaderListSize)
		KeCrash("Failed to read program headers: %d", Status);
	
	uintptr_t BoronDllBase = 0;
	
	uintptr_t BaseAddr, LargestAddr;
	BaseAddr = ~0U;
	LargestAddr = 0;
	
	// Okay, we read the program headers.  Now map each of them in.
	for (int i = 0; i < ElfHeader.ProgramHeaderCount; i++)
	{
		PELF_PROGRAM_HEADER ProgramHeader = (void*) (ProgramHeaders + i * ElfHeader.ProgramHeaderSize);
		
		if (BaseAddr > ProgramHeader->VirtualAddress)
			BaseAddr = ProgramHeader->VirtualAddress;
		if (LargestAddr < ProgramHeader->VirtualAddress + ProgramHeader->SizeInMemory)
			LargestAddr = ProgramHeader->VirtualAddress + ProgramHeader->SizeInMemory;
	}
	
	if (BaseAddr > LargestAddr)
		KeCrash("Error, base address %p > largest address %p", BaseAddr, LargestAddr);
	
	uintptr_t Size = LargestAddr - BaseAddr;
	Size = (Size + PAGE_SIZE - 1) & (PAGE_SIZE - 1);
	
	// Now, the PEB will occupy however many pages, so we will need to cut those out too.
	size_t PebSize = (sizeof(PEB) + PAGE_SIZE - 1) & (PAGE_SIZE - 1);
	
	BoronDllBase = MM_USER_SPACE_END + 1 - PebSize - Size;
	
	// TODO
	(void) BoronDllBase;
	
	PsTerminateThread();
}


INIT
bool PsInitSystemPart2()
{
	return true;
	
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
