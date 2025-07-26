/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	rtl/elf.c
	
Abstract:
	This module implements methods for ELF loading used
	by both the driver loader and the initial process
	loader.
	
Author:
	iProgramInCpp - 19 April 2025
***/
#include <rtl/elf.h>
#include <rtl/assert.h>
#include <string.h>

#ifdef KERNEL
#include <ke/dbg.h>
#endif

#ifdef DEBUG2
#define DbgPrint2(...) DbgPrint(__VA_ARGS__)
#else
#define DbgPrint2(...)
#endif

#ifdef IS_BORON_DLL

extern bool OSDLLAddDllToLoad(const char* DllName);

extern uintptr_t OSDLLGetProcedureAddress(const char* ProcName);

#endif

static bool RtlpComputeRelocation(
	uint32_t Type,
	uintptr_t Addend,
	uintptr_t Base,
	uintptr_t Symbol,
	uintptr_t Place UNUSED,
	uintptr_t* Value,
	uintptr_t* Length
)
{
	*Length = 0;
	*Value  = 0;
	
	switch (Type)
	{
#ifdef TARGET_AMD64
		case R_X86_64_64:
			*Value  = Addend + Symbol;
			*Length = sizeof(uint64_t);
			break;
		case R_X86_64_32:
			*Value  = Addend + Symbol;
			*Length = sizeof(uint32_t);
			break;
		case R_X86_64_RELATIVE:
			*Value  = Base + Addend;
			*Length = sizeof(uint64_t);
			break;
		case R_X86_64_GLOB_DAT:
			*Value  = Symbol;
			*Length = sizeof(uint64_t);
			break;
		case R_X86_64_JUMP_SLOT:
			*Value  = Symbol;
			*Length = sizeof(uint64_t);
			break;
		// I prefer to go here with the "Add as you go with no plan" method
#else
#error Hey! Add ELF relocation types here
#endif
		default:
			DbgPrint("RtlpComputeRelocation: Unknown relocation type %u", Type);
			return false;
	}
	
	return true;
}

static uintptr_t RtlpResolveSymbolAddress(PELF_DYNAMIC_INFO DynInfo, int SymbolIndex, uintptr_t LoadBase)
{
	PELF_SYMBOL ElfSymbol = &DynInfo->DynSymTable[SymbolIndex];
	
	if (ElfSymbol->Value)
		return LoadBase + ElfSymbol->Value;
	
	// Resolve it externally
	const char *SymbolName = DynInfo->DynStrTable + ElfSymbol->Name;
	
#ifdef KERNEL
	// Try linking against the kernel
	uintptr_t Address = DbgLookUpAddress(SymbolName);
	if (!Address)
		KeCrashBeforeSMPInit("ERROR (BoronKernel): Lookup of function %s failed (offs %d value %d index %d)", SymbolName, ElfSymbol->Name, ElfSymbol->Value, SymbolIndex);
	
	return Address;
#else
	// TODO
	DbgPrint("WARNING (Libboron): We don't know how to resolve external symbol %s right now", SymbolName);
	return 0;
#endif
}

static bool RtlpApplyRelocation(
	PELF_DYNAMIC_INFO DynInfo,
	PELF_REL PtrRel,
	PELF_RELA PtrRela,
	uintptr_t LoadBase,
	uintptr_t ResolvedSymbol)
{
	if (!PtrRel && !PtrRela)
		return false;
	
	ELF_RELA Rela;
	if (PtrRela)
	{
		Rela = *PtrRela;
	}
	else
	{
		Rela.Addend = 0;
		Rela.Offset = PtrRel->Offset;
		Rela.Info   = PtrRel->Info;
	}
	
	// If there is no pre-resolved symbol and we actually have a symbol index, then look it up
	if (ResolvedSymbol == 0 && (Rela.Info >> 32) != 0)
		ResolvedSymbol = RtlpResolveSymbolAddress(DynInfo, Rela.Info >> 32, LoadBase);
	
	uintptr_t Place  = LoadBase + Rela.Offset;
	uintptr_t Addend = Rela.Addend;
	
	uintptr_t Value, Length;
	uint32_t RelType = (uint32_t) Rela.Info; // ELF64_R_TYPE(x) => (x & 0xFFFFFFFF)
	
	if (!RtlpComputeRelocation(RelType,
	                           Addend,
	                           LoadBase,
	                           ResolvedSymbol,
	                           Place,
	                           &Value,
	                           &Length))
	{
		DbgPrint("RtlpApplyRelocation: Main relocation failed to compute!");
		return false;
	}
	
#ifdef DEBUG
	if (Length > sizeof(uintptr_t))
		DbgPrint("RtlpApplyRelocation: Length %zu bigger than %zu??", Length, sizeof(uintptr_t));
#endif
	
	memcpy((void*) Place, &Value, Length);
	return true;
}

bool RtlPerformRelocations(PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase)
{
	for (size_t i = 0; i < DynInfo->RelaCount; i++)
	{
		PELF_RELA Rela = &DynInfo->RelaEntries[i];
		if (!RtlpApplyRelocation(DynInfo, NULL, Rela, LoadBase, 0))
			return false;
	}
	
	for (size_t i = 0; i < DynInfo->RelCount; i++)
	{
		PELF_REL Rel = &DynInfo->RelEntries[i];
		if (!RtlpApplyRelocation(DynInfo, Rel, NULL, LoadBase, 0))
			return false;
	}
	
	return true;
}

// Parses the dynamic table of an ELF file and fills in the relevant info in the
// ELF_DYNAMIC_INFO structure.
//
// Context is used by libboron.so to know which DLL is being loaded when a DYN_NEEDED
// is found.
bool RtlParseDynamicTable(PELF_DYNAMIC_ITEM DynItem, PELF_DYNAMIC_INFO Info, uintptr_t LoadBase)
{
	memset(Info, 0, sizeof *Info);
	
#ifdef IS_BORON_DLL
	PELF_DYNAMIC_ITEM Backup = DynItem;
#endif
	
	for (; DynItem->Tag != DYN_NULL; DynItem++)
	{
		switch (DynItem->Tag)
		{
			default:
				DbgPrint2("Dynamic entry with tag %d %x ignored", DynItem->Tag, DynItem->Tag);
				break;
			
			case DYN_NEEDED:
#ifdef KERNEL
				DbgPrint("DYN_NEEDED not supported - libboron.so and drivers may not have other DLLs as dependencies");
				return false;
#endif
				break;
			
			case DYN_REL:
				Info->RelEntries = (ELF_REL*)(LoadBase + DynItem->Pointer);
				break;
			
			case DYN_RELSZ:
				Info->RelCount = DynItem->Pointer / sizeof(ELF_REL);
				break;
			
			case DYN_RELA:
				Info->RelaEntries = (ELF_RELA*)(LoadBase + DynItem->Pointer);
				break;
			
			case DYN_RELASZ:
				Info->RelaCount = DynItem->Pointer / sizeof(ELF_RELA);
				break;
				
			case DYN_RELR:
				Info->RelrEntries = (uintptr_t*)(LoadBase + DynItem->Pointer);
				break;
				
			case DYN_RELRSZ:
				Info->RelrCount = DynItem->Pointer / sizeof(uintptr_t);
				break;
			
			case DYN_STRTAB:
				Info->DynStrTable = (const char*)(LoadBase + DynItem->Pointer);
				break;
			
			case DYN_SYMTAB:
				Info->DynSymTable = (ELF_SYMBOL*)(LoadBase + DynItem->Pointer);
				break;
			
			case DYN_JMPREL:
				Info->PltRelocations = (ELF_SYMBOL*)(LoadBase + DynItem->Pointer);
				break;
			
			case DYN_PLTRELSZ: // don't know why this affects JMPREL and not PLTREL like a sane person
				Info->PltRelocationCount = DynItem->Value;
				break;
			
			case DYN_PLTREL:
				Info->PltUsesRela = DynItem->Value == DYN_RELA;
				break;
				
			case DYN_HASH:
				Info->HashTable = (ELF_HASH_TABLE*)(LoadBase + DynItem->Pointer);
				break;
		}
	}
	
#ifdef IS_BORON_DLL
	// Resolve DLLs to load from DYN_NEEDED.  This is because DYN_NEEDED
	// items may come before DYN_STRTAB is specified.
	DynItem = Backup;
	
	for (; DynItem->Tag != DYN_NULL; DynItem++)
	{
		if (DynItem->Tag != DYN_NEEDED)
			continue;
		
		if (!OSDLLAddDllToLoad(Info->DynStrTable + DynItem->Pointer))
			return false;
	}
#endif
	
	return true;
}

bool RtlLinkPlt(PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase, UNUSED const char* FileName)
{
	const size_t Increment = DynInfo->PltUsesRela ? sizeof(ELF_RELA) : sizeof(ELF_REL);
	
	for (size_t i = 0; i < DynInfo->PltRelocationCount; i += Increment)
	{
		// NOTE: PELF_RELA and PELF_REL need to have the same starting members!!
		PELF_REL Rel = (PELF_REL)((uintptr_t)DynInfo->PltRelocations + i);
		
		PELF_SYMBOL Symbol = &DynInfo->DynSymTable[Rel->Info >> 32];
		
		uintptr_t SymbolOffset = Symbol->Name;
		const char* SymbolName = DynInfo->DynStrTable + SymbolOffset;
		
		uintptr_t SymbolAddress = 0;
		
		if (Symbol->Value)
			SymbolAddress = LoadBase + Symbol->Value;
		
		// If this offset is zero then look it up.
		if (!SymbolAddress)
		{
#ifdef KERNEL
			SymbolAddress = DbgLookUpAddress(SymbolName);
#else
			SymbolAddress = OSDLLGetProcedureAddress(SymbolName);
#endif
		}
		
#ifdef KERNEL
		if (!SymbolAddress)
			KeCrashBeforeSMPInit("RtlLinkPlt: Module %s: lookup of function %s failed (Offset: %zu)", FileName, SymbolName, SymbolOffset);
#else
		ASSERT(SymbolAddress);
		if (!SymbolAddress)
			return false;
#endif
		
		if (!RtlpApplyRelocation(DynInfo,
		                        DynInfo->PltUsesRela ? Rel : NULL,
		                        DynInfo->PltUsesRela ? NULL : (PELF_RELA)Rel,
		                        LoadBase,
		                        SymbolAddress))
			return false;
	}
	
	return true;
}

// TODO: Is this even needed? We aren't using it now
bool RtlUpdateGlobalOffsetTable(uintptr_t *Got, size_t Size, uintptr_t LoadBase)
{
	// Note: A check for 0 here would be redundant as the contents of the
	// for loop just wouldn't execute if the size was zero
	for (size_t i = 0; i < Size; i++) {
		Got[i] += LoadBase;
	}
	
	return true;
}

// The symbol table is used for debugging purposes.  Specifically, this is used by the kernel's
// crash handler to find symbols within loaded kernel shared objects.
void RtlFindSymbolTable(uint8_t* FileAddress, PELF_DYNAMIC_INFO DynInfo)
{
	PELF_SECTION_HEADER SymTabSection = NULL, StrTabSection = NULL;
	PELF_HEADER Header = (PELF_HEADER) FileAddress;
	uintptr_t Offset = (uintptr_t) FileAddress + Header->SectionHeadersOffset;
	
	PELF_SECTION_HEADER SHStrHeader = (PELF_SECTION_HEADER)(FileAddress + Header->SectionHeadersOffset + Header->SectionHeaderNameIndex * Header->SectionHeaderSize);
	const char* SHStringTable = (const char*)(FileAddress + SHStrHeader->OffsetInFile);
	
	for (int i = 0; i < Header->SectionHeaderCount; i++)
	{
		PELF_SECTION_HEADER SectionHeader = (PELF_SECTION_HEADER) Offset;
		Offset += Header->SectionHeaderSize;
		
		// Seems like we gotta grab the name. But we have grabbed the string table already
		const char* SectName = &SHStringTable[SectionHeader->Name];
		
		if (strcmp(SectName, ".symtab") == 0)
			SymTabSection = SectionHeader;
		else if (strcmp(SectName, ".strtab") == 0)
			StrTabSection = SectionHeader;
	}
	
	if (SymTabSection)
	{
		DynInfo->SymbolTable     = (PELF_SYMBOL)(FileAddress + SymTabSection->OffsetInFile);
		DynInfo->SymbolTableSize = SymTabSection->Size / sizeof(ELF_SYMBOL);
	}
	
	if (StrTabSection)
	{
		DynInfo->StringTable = (const char*)(FileAddress + StrTabSection->OffsetInFile);
	}
}

void RtlRelocateRelrEntries(PELF_DYNAMIC_INFO DynInfo, uintptr_t ImageBase)
{
	// Borrowed from mlibc (https://github.com/managarm/mlibc).
	uintptr_t* CurrentBaseAddress = NULL;
	
	for (size_t i = 0; i < DynInfo->RelrCount; i++)
	{
		uintptr_t Rel = DynInfo->RelrEntries[i];
		
		if (Rel & 1)
		{
			// Skip the first bit because we have already relocated
			// the first entry.
			Rel >>= 1;
			
			for (; Rel; Rel >>= 1)
			{
				if (Rel & 1)
					*CurrentBaseAddress += ImageBase;
				
				CurrentBaseAddress++;
			}
		}
		else
		{
			// Even entry indicates the beginning address of the range
			// to relocate.
			CurrentBaseAddress = (uintptr_t*) (ImageBase + Rel);
			
			// Then relocate the first entry.
			*CurrentBaseAddress += ImageBase;
			CurrentBaseAddress++;
		}
	}
}

uint32_t RtlElfHash(const char* Name)
{
	const uint8_t* NameU = (const uint8_t*) Name;
	
	uint32_t H = 0, G;
	
	while (*NameU)
	{
		H = (H << 4) + *NameU++;
		
		if ((G = H & 0xF0000000))
			H ^= G >> 24;
		
		H &= ~G;
	}
	
	return H;
}
