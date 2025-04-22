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
	
	if (ResolvedSymbol == 0)
		ResolvedSymbol = DynInfo->DynSymTable[Rela.Info >> 32].Value;
	
	uintptr_t Place  = LoadBase + Rela.Offset;
	uintptr_t Addend = Rela.Addend;
	
	uintptr_t Value, Length;
	uint32_t RelType = (uint32_t) Rela.Info; // ELF64_R_TYPE(x) => (x & 0xFFFFFFFF)
	
	if (!PtrRela)
	{
		if (!RtlpComputeRelocation(RelType,
		                           0,
		                           LoadBase,
		                           ResolvedSymbol,
		                           Place,
		                           &Value,
		                           &Length))
		{
			DbgPrint("RtlpApplyRelocation: REL type relocation failed to compute!");
			return false;
		}
		
	#ifdef DEBUG
		if (Length > sizeof(uintptr_t))
			DbgPrint("RtlpApplyRelocation: Length %zu bigger than %zu??", Length, sizeof(uintptr_t));
	#endif
		
		memcpy(&Addend, (void*) Place, Length);
	}
	
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

bool RtlParseDynamicTable(PELF_DYNAMIC_ITEM DynItem, PELF_DYNAMIC_INFO Info, uintptr_t LoadBase)
{
	memset(Info, 0, sizeof *Info);
	
	for (; DynItem->Tag != DYN_NULL; DynItem++)
	{
		switch (DynItem->Tag)
		{
			default:
				DbgPrint2("Dynamic entry with tag %d %x ignored", DynItem->Tag, DynItem->Tag);
				break;
			
			case DYN_NEEDED:
				DbgPrint("DYN_NEEDED not supported - libboron.so and drivers may not have other DLLs as dependencies");
				return false;
			
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
		}
	}
	
	return true;
}

bool RtlLinkPlt(PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase, UNUSED bool AllowKernelLinking, UNUSED const char* FileName)
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
			// Kernel drivers can only link against the kernel, and not against each other, for now.
			// Libboron.so cannot link against anyone.
			if (AllowKernelLinking)
			{
				SymbolAddress = DbgLookUpAddress(SymbolName);
			}
			else
			{
				// TODO: Look up in own DLL if needed.  But if the function is in the DLL then
				// the offset shouldn't be zero.
				DbgPrint("ERROR: Cannot link against kernel or another DLL.  Function: %s", SymbolName);
				return false;
			}
#else
			// Libboron.so's librarian knows how to link against other DLLs.
			// TODO
			DbgPrint("ERROR: Need to link against function %s but we don't know how to do that", SymbolName);
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

bool RtlUpdateGlobalOffsetTable(uintptr_t *Got, size_t Size, uintptr_t LoadBase)
{
	// Note: A check for 0 here would be redundant as the contents of the
	// for loop just wouldn't execute if the size was zero
	for (size_t i = 0; i < Size; i++)
		Got[i] += LoadBase;
	
	return true;
}

void RtlParseInterestingSections(uint8_t* FileAddress, PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase)
{
	PELF_SECTION_HEADER GotSection = NULL, GotPltSection = NULL, SymTabSection = NULL, StrTabSection = NULL;
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
		
		if (strcmp(SectName, ".got") == 0)
			GotSection = SectionHeader;
		else if (strcmp(SectName, ".got.plt") == 0)
			GotPltSection = SectionHeader;
		else if (strcmp(SectName, ".symtab") == 0)
			SymTabSection = SectionHeader;
		else if (strcmp(SectName, ".strtab") == 0)
			StrTabSection = SectionHeader;
	}
	
	if (GotSection)
	{
		DynInfo->GlobalOffsetTable     = (uintptr_t*)(LoadBase + GotSection->VirtualAddress);
		DynInfo->GlobalOffsetTableSize = GotSection->Size / sizeof(uintptr_t);
	}
	else
	{
		DynInfo->GlobalOffsetTable     = NULL;
		DynInfo->GlobalOffsetTableSize = 0;
	}
	
	if (GotPltSection)
	{
		DynInfo->GotPlt     = (uintptr_t*)(LoadBase + GotPltSection->VirtualAddress);
		DynInfo->GotPltSize = GotPltSection->Size / sizeof(uintptr_t);
	}
	else
	{
		DynInfo->GotPlt     = NULL;
		DynInfo->GotPltSize = 0;
	}
	
	if (SymTabSection)
	{
		DynInfo->SymbolTable     = (PELF_SYMBOL)(FileAddress + SymTabSection->OffsetInFile);
		DynInfo->SymbolTableSize = SymTabSection->Size / sizeof(ELF_SYMBOL);
	}
	
	if (StrTabSection)
		DynInfo->StringTable = (const char*)(FileAddress + StrTabSection->OffsetInFile);
}
