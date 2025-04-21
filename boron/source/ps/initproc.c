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

BSTATUS PspLdrParseInterestingSections(HANDLE FileHandle, PELF_HEADER Header, PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase)
{
	BSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	size_t SHdrTableSize;
	void* SectionHeaders = NULL;
	char* SHStringTable = NULL;
	PELF_SECTION_HEADER GotSection = NULL;
	PELF_SECTION_HEADER GotPltSection = NULL;
	//PELF_SECTION_HEADER SymTabSection = NULL;
	//PELF_SECTION_HEADER StrTabSection = NULL;
	
	SHdrTableSize = Header->SectionHeaderCount * Header->SectionHeaderSize;
	SectionHeaders = MmAllocatePool(POOL_PAGED, SHdrTableSize);
	if (!SectionHeaders)
		return STATUS_INSUFFICIENT_MEMORY;
	
	Status = OSSeekFile(FileHandle, Header->SectionHeadersOffset, IO_SEEK_SET);
	if (FAILED(Status))
		goto Exit;
	
	memset(&Iosb, 0, sizeof Iosb);
	Status = OSReadFile(&Iosb, FileHandle, SectionHeaders, SHdrTableSize, 0);
	if (FAILED(Status) || Iosb.BytesRead != SHdrTableSize)
		goto Exit;
	
	// Okay, so we grabbed the section header.
	// Now, find the .shstrtab (section header string table)
	PELF_SECTION_HEADER SHStrHeader = (PELF_SECTION_HEADER)((uintptr_t)SectionHeaders + Header->SectionHeaderNameIndex * Header->SectionHeaderSize);
	SHStringTable = MmAllocatePool(POOL_PAGED, SHStrHeader->Size);
	if (!SHStringTable)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Exit;
	}
	
	// Seek there, and load.
	Status = OSSeekFile(FileHandle, SHStrHeader->OffsetInFile, IO_SEEK_SET);
	if (FAILED(Status))
		goto Exit;
	
	Status = OSReadFile(&Iosb, FileHandle, SHStringTable, SHStrHeader->Size, 0);
	if (FAILED(Status) || Iosb.BytesRead != SHStrHeader->Size)
		goto Exit;
	
	// OK, now we read the section header string table.
	// Now, we can *finally* look for the GOT section.
	uint8_t* Offset = SectionHeaders;
	
	for (int i = 0; i < Header->SectionHeaderCount; i++)
	{
		PELF_SECTION_HEADER SectionHeader = (PELF_SECTION_HEADER) Offset;
		Offset += Header->SectionHeaderSize;
		
		// Seems like we gotta grab the name. But we have grabbed the string table
		const char* SectName = &SHStringTable[SectionHeader->Name];
		
		// N.B. We may need symtab and strtab in the future.
		if (strcmp(SectName, ".got") == 0)
			GotSection = SectionHeader;
		else if (strcmp(SectName, ".got.plt") == 0)
			GotPltSection = SectionHeader;
		//else if (strcmp(SectName, ".symtab") == 0)
		//	SymTabSection = SectionHeader;
		//else if (strcmp(SectName, ".strtab") == 0)
		//	StrTabSection = SectionHeader;
	}
	
	if (GotSection)
	{
		DynInfo->GlobalOffsetTable = (uintptr_t*) (LoadBase + GotSection->VirtualAddress);
		DynInfo->GlobalOffsetTableSize = GotSection->Size / sizeof(uintptr_t);
	}
	else
	{
		DynInfo->GlobalOffsetTable = NULL;
		DynInfo->GlobalOffsetTableSize = 0;
	}
	
	if (GotPltSection)
	{
		DynInfo->GotPlt = (uintptr_t*) (LoadBase + GotPltSection->VirtualAddress);
		DynInfo->GotPltSize = GotPltSection->Size / sizeof(uintptr_t);
	}
	else
	{
		DynInfo->GotPlt = NULL;
		DynInfo->GotPltSize = 0;
	}
	
	// TODO: Do we need to use the symtab and strtab sections?
	
Exit:
	if (SHStringTable)
		MmFreePool(SHStringTable);
	
	MmFreePool(SectionHeaders);
	return Status;
}

NO_RETURN
void PspUserThreadStart(void* Context)
{
	// TODO: Actually move to user mode.
	
	void (*Ptr)();
	Ptr = (void(*)())Context;
	
	DbgPrint("Ptr:%p",Ptr);
	Ptr();
	
	PsTerminateThread();
}

NO_RETURN
void PsStartInitialProcess(UNUSED void* Context)
{
	BSTATUS Status;
	HANDLE ProcessHandle;
	HANDLE FileHandle;
	OBJECT_ATTRIBUTES FileAttributes;
	IO_STATUS_BLOCK Iosb;
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
		KeCrash("%s: Failed to create initial process: %d", Status);
	
	FileAttributes.RootDirectory = HANDLE_NONE;
	FileAttributes.ObjectName = PspInitialProcessFileName;
	FileAttributes.ObjectNameLength = strlen(PspInitialProcessFileName);
	FileAttributes.OpenFlags = 0;
	
	Status = OSOpenFile(
		&FileHandle,
		&FileAttributes
	);
	
	if (FAILED(Status))
		KeCrash("%s: Failed to open %s: %d", Func, PspInitialProcessFileName, Status);
	
	// The file has been opened.
	// Read the ELF header.
	memset(&Iosb, 0, sizeof Iosb);
	Status = OSReadFile(&Iosb, FileHandle, &ElfHeader, sizeof(ELF_HEADER), 0);
	
	if (FAILED(Status) || Iosb.BytesRead < sizeof(ELF_HEADER))
		KeCrash("%s: Failed to read ELF header: %d", Func, Status);
	
	// TODO: Check if you are a valid ELF header.
	
	// Read program header information.
	size_t ProgramHeaderListSize = ElfHeader.ProgramHeaderSize * ElfHeader.ProgramHeaderCount;
	uint8_t* ProgramHeaders = MmAllocatePool(POOL_PAGED, ProgramHeaderListSize);
	if (!ProgramHeaders)
		KeCrash("%s: Out of memory while trying to allocate program headers", Func);
	
	Status = OSSeekFile(FileHandle, ElfHeader.ProgramHeadersOffset, IO_SEEK_SET);
	if (FAILED(Status))
		KeCrash("%s: Failed to seek to program headers offset", Func);
	
	Status = OSReadFile(&Iosb, FileHandle, ProgramHeaders, ProgramHeaderListSize, 0);
	if (FAILED(Status) || Iosb.BytesRead < ProgramHeaderListSize)
		KeCrash("%s: Failed to read program headers: %d", Func, Status);
	
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
		
		DbgPrint("BaseAddress: %p   DynPhdrVAddr: %p", BaseAddress, DynamicBaseAddress);
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
		
		DbgPrint(
			"--New PHdr\n"
			"\tProgramHeader->SizeInMemory: %zu\n"
			"\tProgramHeader->VirtualAddress: %p\n"
			"\tProgramHeader->Offset: %p",
			ProgramHeader->SizeInMemory,
			ProgramHeader->VirtualAddress,
			ProgramHeader->Offset
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
	DbgPrint("BoronDllBase : %p", BoronDllBase);
	
	// This assumes that the dynamic table is entirely mapped.
	if (!IsDynamicLoaded)
		KeCrash("%s: There is no LOAD segment that maps the DYNAMIC table.", Func);
	
	// Attach to this process.
	PEPROCESS Process = NULL;
	Status = ExReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, (void**) &Process);
	ASSERT(SUCCEEDED(Status));
	
	PsAttachToProcess(Process);
	
	ELF_DYNAMIC_INFO DynInfo;
	PELF_DYNAMIC_ITEM DynTable = (PELF_DYNAMIC_ITEM) (BoronDllBase + DynamicPhdr->VirtualAddress);
	
	if (!RtlParseDynamicTable(DynTable, &DynInfo, BoronDllBase))
		KeCrash("%s: Failed to parse dynamic table.", Func);
	
	if (!RtlPerformRelocations(&DynInfo, BoronDllBase))
		KeCrash("%s: Failed to perform relocations.", Func);
	
	Status = PspLdrParseInterestingSections(FileHandle, &ElfHeader, &DynInfo, BoronDllBase);
	if (FAILED(Status))
		KeCrash("%s: Failed to read section header: %d (%s)", Func, Status, RtlGetStatusString(Status));
	
	RtlUpdateGlobalOffsetTable(DynInfo.GlobalOffsetTable, DynInfo.GlobalOffsetTableSize, BoronDllBase);
	RtlUpdateGlobalOffsetTable(DynInfo.GotPlt, DynInfo.GotPltSize, BoronDllBase);
	
	// TODO: Adjust permissions so that the code segment is not writable anymore even with CoW.
	
	PsDetachFromProcess();
	ObDereferenceObject(Process);
	OSClose(FileHandle);
	MmFreePool(ProgramHeaders);
	
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
