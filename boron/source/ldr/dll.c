/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ldr/loader.c
	
Abstract:
	This module implements the driver DLL loader.
	
Author:
	iProgramInCpp - 22 October 2023
***/
#include "ldri.h"
#include "elf.h"

#ifdef DEBUG2
#define DbgPrint2(...) DbgPrint(__VA_ARGS__)
#else
#define DbgPrint2(...)
#endif

static void LdriMapInProgramHeader(PLIMINE_FILE File, PELF_PROGRAM_HEADER Phdr)
{
	// Note! We can modify the file.  So we will.
	size_t SizePages = (Phdr->SizeInMemory + PAGE_SIZE - 1) / PAGE_SIZE;
	uintptr_t VirtAddr = LdrAllocateRange(SizePages);
	
	Phdr->VirtualAddress = VirtAddr;
	
	uintptr_t PhyAddr = (uintptr_t)(File + Phdr->Offset) & ~0xFFF;
	
	uintptr_t Permissions = MM_PTE_SUPERVISOR;
	
	if (~Phdr->Flags & ELF_PHDR_EXEC)
		Permissions |= MM_PTE_NOEXEC;
	if (Phdr->Flags & ELF_PHDR_WRITE)
		Permissions |= MM_PTE_READWRITE;
	
	// Now map it in.
	for (size_t i = 0; i < SizePages; i++)
	{
		if (!MmMapPhysicalPage(MmGetCurrentPageMap(),
		                       PhyAddr,
		                       VirtAddr,
		                       Permissions))
		{
			DbgPrint("ERROR: Can't map physical page from %p to %p!", PhyAddr, VirtAddr);
			return;
		}
		
		PhyAddr  += PAGE_SIZE;
		VirtAddr += PAGE_SIZE;
	}
	
}

void LdriLoadDll(PLIMINE_FILE File)
{
	PELF_HEADER Header = (PELF_HEADER) File->address;
	
	DbgPrint("Loading module %s (Address %p)", File->path, File);
	
	if (Header->Type != ELF_TYPE_DYNAMIC)
	{
		DbgPrint("Error, module %s is NOT a DLL!", File->path);
		return;
	}
	
	DbgPrint2("num phdrs: %d", Header->ProgramHeaderCount);
	DbgPrint2("num shdrs: %d", Header->SectionHeaderCount);
	DbgPrint2("offs phdr: %d", Header->ProgramHeadersOffset);
	DbgPrint2("offs shdr: %d", Header->SectionHeadersOffset);
	DbgPrint2("flags: %08x", Header->Flags);
	DbgPrint2("hdrsz: %d", Header->HeaderSize);
	DbgPrint2("etype: %d", Header->Type);
	DbgPrint2("machi: %d", Header->Machine);
	DbgPrint2("versn: %d", Header->Version);

	// Load program headers into memory.
	uintptr_t Offset = (uintptr_t) File->address + Header->ProgramHeadersOffset;
	DbgPrint2("Program headers:");
	DbgPrint2("Type     Offset           VirtAddr         PhysAddr         FileSize         MemSize          Flags    Align");
	//DbgPrint("00000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 00000000 00000000");
	for (int i = 0; i < Header->ProgramHeaderCount; i++)
	{
		PELF_PROGRAM_HEADER ProgramHeader = (PELF_PROGRAM_HEADER) Offset;
		Offset += Header->ProgramHeaderSize;
		
		DbgPrint2("%08x %p %p %p %p %p %08x %08x",
		          ProgramHeader->Type,
		          ProgramHeader->Offset,
		          ProgramHeader->VirtualAddress,
		          ProgramHeader->PhysicalAddress,
		          ProgramHeader->SizeInFile,
		          ProgramHeader->SizeInMemory,
		          ProgramHeader->Flags,
		          ProgramHeader->Alignment);
		
		LdriMapInProgramHeader(File, ProgramHeader);
	}
}
