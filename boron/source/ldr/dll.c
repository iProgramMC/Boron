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

// TODO: Update operations with regards to relocations in case we're porting to a 32-bit platform.

// Big thanks to https://github.com/DeanoBurrito/northport for help implementing the dynamic linker.

#ifdef DEBUG2
#define DbgPrint2(...) DbgPrint(__VA_ARGS__)
#else
#define DbgPrint2(...)
#endif

extern uintptr_t MiAllocatePageFromMemMap();

LOADED_DLL KeLoadedDLLs[256];
int        KeLoadedDLLCount = 0;

// Note! BaseOffset - The offset at which the ELF was loaded.  It's equal to VirtualAddr - AddressOfLowestPHDR
static void LdriMapInProgramHeader(PLIMINE_FILE File, PELF_PROGRAM_HEADER Phdr, uintptr_t BaseVirtAddr)
{
	if (Phdr->SizeInMemory == 0)
		return;
	
	const int PageMask = PAGE_SIZE - 1;
	
	// Store the virtual address (here it's actually the offset from the image base) for later use.
	uintptr_t Offset = Phdr->VirtualAddress;
	uintptr_t VirtAddr = (BaseVirtAddr + Offset) & ~PageMask, Padding = (BaseVirtAddr + Offset) & PageMask;
	
	// Calculate the size of the total allocation.
	size_t SizePages = (Phdr->SizeInMemory + Padding + PAGE_SIZE - 1) / PAGE_SIZE;
	
	uintptr_t Permissions = MM_PTE_SUPERVISOR;
	if (~Phdr->Flags & ELF_PHDR_EXEC)
		Permissions |= MM_PTE_NOEXEC;
	
	Permissions |= MM_PTE_READWRITE;
	
	HPAGEMAP PageMap = MmGetCurrentPageMap();
	
	// Now map it in.
	uintptr_t VirtAddrBackup = VirtAddr;
	for (size_t i = 0; i < SizePages; i++)
	{
		// NOTE: I should just map the module directly, however, the
		// BSS doesn't actually occupy any space in the file itself,
		// so this needs to be done.
		
		// Normally in a mmap based elf loader, mmap itself would take
		// care of zero-filling everything. But not here.
		
		// Some entries overlap. Check if there's already a PTE beforehand.
		PMMPTE Pte = MmGetPTEPointer(PageMap, VirtAddr, false);
		if (Pte && (*Pte & MM_PTE_PRESENT))
		{
			MMPTE OldPte = *Pte;
			
			// Update the permissions. There shouldn't really be any
			// execute segments inside of an NX segment, or reverse...
			uintptr_t PermsWithoutNX = Permissions & ~MM_PTE_NOEXEC;
			*Pte |= PermsWithoutNX;
			
			if (~Permissions & MM_PTE_NOEXEC)
				*Pte &= ~MM_PTE_NOEXEC;
			
			// If the PTE has changed, invalidate. We aren't ready to take
			// page faults at this point, even if they'd be solved quickly.
			if (OldPte != *Pte)
				KeInvalidatePage((void*) VirtAddr);
			
			continue;
		}
		
		int Pfn = MmAllocatePhysicalPage();
		if (Pfn == PFN_INVALID)
			KeCrashBeforeSMPInit("Out of memory trying to map program header!");
		
		uintptr_t Page = MmPFNToPhysPage(Pfn);
		
		if (!MmMapPhysicalPage(PageMap,
		                       Page,
		                       VirtAddr,
		                       Permissions))
			KeCrashBeforeSMPInit("Can't map in program header to virtual address %p!", VirtAddr);
		
		VirtAddr += PAGE_SIZE;
	}
	
	VirtAddr = VirtAddrBackup;
	
	// Clear the segment.
	memset((void*)(BaseVirtAddr + Offset), 0, Phdr->SizeInMemory);
	
	// Copy in the data.
	memcpy((void*)(BaseVirtAddr + Offset),
	       (void*)((uintptr_t) File->address + Phdr->Offset),
	       Phdr->SizeInFile);
}

static void LdrpGetLimits(PLIMINE_FILE File, uintptr_t* BaseAddr, uintptr_t* LargestAddr)
{
	*BaseAddr = ~0U;
	*LargestAddr = 0;
	
	PELF_HEADER Header = (PELF_HEADER) File->address;
	uintptr_t Offset = (uintptr_t) File->address + Header->ProgramHeadersOffset;
	
	for (int i = 0; i < Header->ProgramHeaderCount; i++)
	{
		PELF_PROGRAM_HEADER ProgramHeader = (PELF_PROGRAM_HEADER) Offset;
		Offset += Header->ProgramHeaderSize;
		
		if (*BaseAddr > ProgramHeader->VirtualAddress)
			*BaseAddr = ProgramHeader->VirtualAddress;
		if (*LargestAddr < ProgramHeader->VirtualAddress + ProgramHeader->SizeInMemory)
			*LargestAddr = ProgramHeader->VirtualAddress + ProgramHeader->SizeInMemory;
	}
}

static bool LdrpParseDynamicTable(PLIMINE_FILE File, PELF_PROGRAM_HEADER DynamicPhdr, PELF_DYNAMIC_INFO Info, uintptr_t LoadBase)
{
	memset(Info, 0, sizeof *Info);
	//PELF_HEADER Header = (PELF_HEADER) File->address;
	PELF_DYNAMIC_ITEM DynItem = (PELF_DYNAMIC_ITEM) (File->address + DynamicPhdr->Offset);
	
	for (; DynItem->Tag != DYN_NULL; DynItem++)
	{
		switch (DynItem->Tag)
		{
			default:
				DbgPrint2("Dynamic entry with tag %d %x ignored", DynItem->Tag, DynItem->Tag);
				break;
			
			case DYN_NEEDED:
				DbgPrint("DYN_NEEDED not supported - a DLL may not have other DLLs as dependencies");
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
				Info->StringTable = (const char*)(LoadBase + DynItem->Pointer);
				break;
			
			case DYN_SYMTAB:
				Info->SymbolTable = (ELF_SYMBOL*)(LoadBase + DynItem->Pointer);
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

static bool LdrpComputeRelocation(
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
		case R_X86_64_JUMP_SLOT:
			*Value  = Symbol;
			*Length = sizeof(uint64_t);
			break;
		// I prefer to go here with the "Add as you go with no plan" method
#else
#error Hey! Add ELF relocation types here
#endif
		default:
			DbgPrint("LdrpComputeRelocation: Unknown relocation type %u", Type);
			return false;
	}
	
	return true;
}

static bool LdrpApplyRelocation(PELF_DYNAMIC_INFO DynInfo, PELF_REL PtrRel, PELF_RELA PtrRela, uintptr_t LoadBase, uintptr_t ResolvedSymbol)
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
		ResolvedSymbol = DynInfo->SymbolTable[Rela.Info >> 32].Value;
	
	uintptr_t Place  = LoadBase + Rela.Offset;
	uintptr_t Addend = Rela.Addend;
	
	uintptr_t Value, Length;
	uint32_t RelType = (uint32_t) Rela.Info; // ELF64_R_TYPE(x) => (x & 0xFFFFFFFF)
	
	if (!PtrRela)
	{
		if (!LdrpComputeRelocation(RelType,
		                           0,
		                           LoadBase,
		                           ResolvedSymbol,
		                           Place,
		                           &Value,
		                           &Length))
		{
			DbgPrint("LdrpApplyRelocation: REL type relocation failed to compute!");
			return false;
		}
		
	#ifdef DEBUG
		if (Length > sizeof(uintptr_t))
			DbgPrint("LdrpApplyRelocation: Length %zu bigger than %zu??", Length, sizeof(uintptr_t));
	#endif
		
		memcpy(&Addend, (void*) Place, Length);
	}
	
	if (!LdrpComputeRelocation(RelType,
	                           Addend,
	                           LoadBase,
	                           ResolvedSymbol,
	                           Place,
	                           &Value,
	                           &Length))
	{
		DbgPrint("LdrpApplyRelocation: Main relocation failed to compute!");
		return false;
	}
	
#ifdef DEBUG
	if (Length > sizeof(uintptr_t))
		DbgPrint("LdrpApplyRelocation: Length %zu bigger than %zu??", Length, sizeof(uintptr_t));
#endif
	
	memcpy((void*) Place, &Value, Length);
	
	return true;
}

static bool LdrpPerformRelocations(PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase)
{
	for (size_t i = 0; i < DynInfo->RelaCount; i++)
	{
		PELF_RELA Rela = &DynInfo->RelaEntries[i];
		if (!LdrpApplyRelocation(DynInfo, NULL, Rela, LoadBase, 0))
			return false;
	}
	
	for (size_t i = 0; i < DynInfo->RelCount; i++)
	{
		PELF_REL Rel = &DynInfo->RelEntries[i];
		if (!LdrpApplyRelocation(DynInfo, Rel, NULL, LoadBase, 0))
			return false;
	}
	
	return true;
}

static bool LdrpLinkPlt(PLIMINE_FILE File, PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase)
{
	const size_t Increment = DynInfo->PltUsesRela ? sizeof(ELF_RELA) : sizeof(ELF_REL);
	
	for (size_t i = 0; i < DynInfo->PltRelocationCount; i += Increment)
	{
		// NOTE: PELF_RELA and PELF_REL need to have the same starting members!!
		PELF_REL Rel = (PELF_REL)((uintptr_t)DynInfo->PltRelocations + i);
		
		uintptr_t SymbolOffset = DynInfo->SymbolTable[Rel->Info >> 32].Name;
		const char* SymbolName = DynInfo->StringTable + SymbolOffset;
		
		uintptr_t Symbol = DbgLookUpAddress(SymbolName);
		
		if (!Symbol)
			KeCrashBeforeSMPInit("LdrLink: Module %s: lookup of function %s failed (Offset: %zu)", File->path, SymbolName, SymbolOffset);
		
		if (!LdrpApplyRelocation(DynInfo,
		                        DynInfo->PltUsesRela ? Rel : NULL,
		                        DynInfo->PltUsesRela ? NULL : (PELF_RELA)Rel,
		                        LoadBase,
		                        Symbol))
			return false;
	}
	
	return true;
}

static PELF_PROGRAM_HEADER LdrpLoadProgramHeaders(PLIMINE_FILE File, uintptr_t *LoadBase)
{
	PELF_HEADER Header = (PELF_HEADER) File->address;
	
	PELF_PROGRAM_HEADER Dynamic = NULL; // needed later
	
	uintptr_t Offset = (uintptr_t) File->address + Header->ProgramHeadersOffset;
	
	// Scan the program headers to determine the size of our memory region.
	uintptr_t BaseAddr, LargestAddr;
	LdrpGetLimits(File, &BaseAddr, &LargestAddr);
	
	if (BaseAddr > LargestAddr)
		KeCrashBeforeSMPInit("LdrpLoadProgramHeaders: Error, base address > largest address");
	
	uintptr_t Size = LargestAddr - BaseAddr;
	
	// Allocate this many pages.
	Size = (Size + PAGE_SIZE - 1) / PAGE_SIZE;
	if (Size == 0)
		KeCrashBeforeSMPInit("LdrpLoadProgramHeaders: Error, size is zero");
	
	uintptr_t VirtualAddr = LdrAllocateRange(Size);
	*LoadBase = VirtualAddr;
	
	// The program headers can be placed anywhere page aligned so long as the PHDRs' position relative to each other isn't changed.
	for (int i = 0; i < Header->ProgramHeaderCount; i++)
	{
		PELF_PROGRAM_HEADER ProgramHeader = (PELF_PROGRAM_HEADER) Offset;
		Offset += Header->ProgramHeaderSize;
		
		LdriMapInProgramHeader(File, ProgramHeader, VirtualAddr);
		
		if (ProgramHeader->Type == PROG_DYNAMIC)
		{
			if (Dynamic)
				KeCrashBeforeSMPInit("LdrpLoadProgramHeaders: More than one dynamic section?!");
			
			Dynamic = ProgramHeader;
		}
	}
	
	return Dynamic;
}

void* LdrpLookUpRoutineAddress(PELF_DYNAMIC_INFO DynInfo, uintptr_t Base, const char* Name)
{
	// TODO: Better way to do this, that doesn't go out of bounds when the symbol isn't found!
	for (PELF_SYMBOL Symbol = DynInfo->SymbolTable; ; Symbol++)
	{
		uintptr_t SymbolOffset = Symbol->Name;
		const char* SymbolName = DynInfo->StringTable + SymbolOffset;
		
		if (strcmp(SymbolName, Name) == 0)
			return (void*)(Base + Symbol->Value);
	}
	
	// TODO!
}

void LdriLoadDll(PLIMINE_FILE File)
{
	if (KeLoadedDLLCount >= (int) ARRAY_COUNT(KeLoadedDLLs))
	{
		DbgPrint("Loaded too many DLLs! Refusing to load more... (%s was ignored)", File->path);
		return;
	}
	
	PLOADED_DLL LoadedDLL = &KeLoadedDLLs[KeLoadedDLLCount++];
	
	LoadedDLL->LimineFile = File;
	LoadedDLL->Name       = File->path;
	
	PELF_HEADER Header = (PELF_HEADER) File->address;
	
	DbgPrint("Loading module %s (Address %p)", File->path, File);
	
	if (Header->Type != ELF_TYPE_DYNAMIC)
		KeCrashBeforeSMPInit("LdriLoadDll: %s: Error, module is not actually a DLL", File->path);
	
	// Load the program headers into memory.
	uintptr_t LoadBase;
	PELF_PROGRAM_HEADER Dynamic = LdrpLoadProgramHeaders(File, &LoadBase);
	
	LoadedDLL->ImageBase = LoadBase;
	
	if (!Dynamic)
		KeCrashBeforeSMPInit("LdriLoadDll: %s: No dynamic table?", File->path);
	
	ELF_DYNAMIC_INFO DynInfo;
	if (!LdrpParseDynamicTable(File, Dynamic, &DynInfo, LoadBase))
		KeCrashBeforeSMPInit("LdriLoadDll: %s: Failed to parse dynamic table", File->path);
	
	if (!LdrpPerformRelocations(&DynInfo, LoadBase))
		KeCrashBeforeSMPInit("LdriLoadDll: %s: Failed to perform relocations", File->path);
	
	if (!LdrpLinkPlt(File, &DynInfo, LoadBase))
		KeCrashBeforeSMPInit("LdriLoadDll: %s: Failed to link with the kernel", File->path);
	
	LoadedDLL->EntryPoint = LdrpLookUpRoutineAddress(&DynInfo, LoadBase, "DriverEntry");
	
	if (!LoadedDLL->EntryPoint)
		KeCrashBeforeSMPInit("LdriLoadDll: %s: Unable to find DriverEntry", File->path);
	
	DbgPrint("Entering entry point: %p", LoadedDLL->EntryPoint);
	
	int Value = LoadedDLL->EntryPoint();
	
	LogMsg("Value that %s returned: %d", File->path, Value);
}
