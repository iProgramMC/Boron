/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ldr/elf.h
	
Abstract:
	This header file contains struct declarations
	
Author:
	iProgramInCpp - 22 October 2023
***/
#ifndef BORON_ELF_H
#define BORON_ELF_H

#include <main.h>

// @WORK: For other platforms
#if defined TARGET_AMD64
#define IS_64_BIT
#endif

typedef NO_RETURN void(*ELF_ENTRY_POINT)();

enum
{
	ELF_IDENT_MAGIC_0,
	ELF_IDENT_MAGIC_1,
	ELF_IDENT_MAGIC_2,
	ELF_IDENT_MAGIC_3,
	ELF_IDENT_CLASS,
	ELF_IDENT_DATA,
	ELF_IDENT_VERSION,
	ELF_IDENT_OS_ABI,
	ELF_IDENT_ABI_VERSION,
};

enum
{
	ELF_TYPE_NONE,
	ELF_TYPE_RELOCATABLE,
	ELF_TYPE_EXECUTABLE,
	ELF_TYPE_DYNAMIC,
	ELF_TYPE_CORE_DUMP,
};

enum
{
	ELF_PHDR_READ  = (1 << 2),
	ELF_PHDR_WRITE = (1 << 1),
	ELF_PHDR_EXEC  = (1 << 0),
};

typedef struct ELF_HEADER_tag
{
	char      Identifier[16];
	uint16_t  Type;
	uint16_t  Machine;
	uint32_t  Version;
	ELF_ENTRY_POINT EntryPoint;
	uintptr_t ProgramHeadersOffset;
	uintptr_t SectionHeadersOffset;
	uint32_t  Flags;
	uint16_t  HeaderSize;
	uint16_t  ProgramHeaderSize;
	uint16_t  ProgramHeaderCount;
	uint16_t  SectionHeaderSize;
	uint16_t  SectionHeaderCount;
	uint16_t  SectionHeaderNameIndex;
}
PACKED
ELF_HEADER, *PELF_HEADER;

typedef struct ELF_PROGRAM_HEADER_tag
{
	uint32_t  Type;
#ifdef IS_64_BIT
	uint32_t  Flags;
#endif
	uintptr_t Offset;
	uintptr_t VirtualAddress;
	uintptr_t PhysicalAddress; // Ignored
	uintptr_t SizeInFile;
	uintptr_t SizeInMemory;
#ifndef IS_64_BIT
	uint32_t  Flags;
#endif
	uint32_t  Alignment;
}
PACKED
ELF_PROGRAM_HEADER, *PELF_PROGRAM_HEADER;

typedef struct ELF_SECTION_HEADER_tag
{
	uint32_t  Name; // Index in the shstrtab section
	uint32_t  Type;
	uintptr_t Flags;
	uintptr_t VirtualAddress;
	uintptr_t OffsetInFile;
	uintptr_t Size;
	uint32_t  Link;
	uint32_t  Info;
	uintptr_t Alignment;
	uintptr_t EntrySize;
}
PACKED
ELF_SECTION_HEADER, *PELF_SECTION_HEADER;

#endif//BORON_ELF_H