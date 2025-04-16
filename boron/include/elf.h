/***
	The Boron Operating System
	Copyright (C) 2023-2025 iProgramInCpp

Module name:
	elf.h
	
Abstract:
	This header file contains struct declarations
	for the ELF file format.
	
Author:
	iProgramInCpp - 22 October 2023
***/
#ifndef BORON_ELF_H
#define BORON_ELF_H

#include <main.h>

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
	DYN_NULL,
	DYN_NEEDED,
	DYN_PLTRELSZ,
	DYN_PLTGOT,
	DYN_HASH,
	DYN_STRTAB,
	DYN_SYMTAB,
	DYN_RELA,
	DYN_RELASZ,
	DYN_RELAENT,
	DYN_STRSZ,
	DYN_SYMENT,
	DYN_INIT,
	DYN_FINI,
	DYN_SONAME,
	DYN_RPATH,
	DYN_SYMBOLIC,
	DYN_REL,
	DYN_RELSZ,
	DYN_RELENT,
	DYN_PLTREL,
	DYN_DEBUG,
	DYN_TEXTREL,
	DYN_JMPREL,
	DYN_BIND_NOW,
	DYN_INIT_ARRAY,
	DYN_FINI_ARRAY,
	DYN_INIT_ARRAYSZ,
	DYN_FINI_ARRAYSZ,
	DYN_RUNPATH,
	DYN_ENCODING = 32,
	DYN_PREINIT_ARRAY = 32, // ignored
	DYN_PREINIT_ARRAYSZ,
	DYN_MAXPOSTAGS,
};

enum
{
	ELF_PHDR_READ  = (1 << 2),
	ELF_PHDR_WRITE = (1 << 1),
	ELF_PHDR_EXEC  = (1 << 0),
};

enum
{
	PROG_NULL,
	PROG_LOAD,
	PROG_DYNAMIC,
	PROG_INTERP,
	PROG_NOTE,
};

enum
{
#ifdef TARGET_AMD64
	R_X86_64_NONE,
	R_X86_64_64,
	R_X86_64_PC32,
	R_X86_64_GOT32,
	R_X86_64_PLT32,
	R_X86_64_COPY,
	R_X86_64_GLOB_DAT,
	R_X86_64_JUMP_SLOT,
	R_X86_64_RELATIVE,
	R_X86_64_GOTPCREL,
	R_X86_64_32,
	R_X86_64_32S,
	R_X86_64_16,
	R_X86_64_16S,
	R_X86_64_8,
	R_X86_64_8S,
	//...
#else
#error Hey! Add ELF relocation types here
#endif
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

typedef struct ELF_DYNAMIC_ITEM_tag
{
	uintptr_t Tag;
	
	union
	{
		intptr_t  Value;
		uintptr_t Pointer;
	};
}
PACKED
ELF_DYNAMIC_ITEM, *PELF_DYNAMIC_ITEM;

typedef struct
{
#ifdef IS_64_BIT
	uint32_t  Name;
	uint8_t   Info;
	uint8_t   Other;
	uint16_t  SectionHeaderIndex;
	uintptr_t Value;
	uintptr_t Size;
#else
	uint32_t  Name;
	uintptr_t Value;
	uintptr_t Size;
	uint8_t   Info;
	uint8_t   Other;
	uint16_t  SectionHeaderIndex;
#endif
}
PACKED
ELF_SYMBOL, *PELF_SYMBOL;

typedef struct
{
	uintptr_t Offset;
	uintptr_t Info;
}
PACKED
ELF_REL, *PELF_REL;

typedef struct
{
	uintptr_t Offset;
	uintptr_t Info;
	intptr_t  Addend;
}
PACKED
ELF_RELA, *PELF_RELA;

#endif//BORON_ELF_H