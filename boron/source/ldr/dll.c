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
#include <rtl/elf.h>

// This would be a layering violation, but this component is only active during system init, so it's OK, I think
//
// Note: DLL loading is done during UP initialization, so there is no need to acquire the kernel space's lock.
#include "../mm/mi.h"

// TODO: Update operations with regards to relocations in case we're porting to a 32-bit platform.

// Big thanks to https://github.com/DeanoBurrito/northport for help implementing the dynamic linker.

#ifdef DEBUG2
#define DbgPrint2(...) DbgPrint(__VA_ARGS__)
#else
#define DbgPrint2(...)
#endif

LOADED_DLL KeLoadedDLLs[256];
int        KeLoadedDLLCount = 0;

static DRIVER_OBJECT LdrpHalReservedDriverObject;

// Note! BaseOffset - The offset at which the ELF was loaded.  It's equal to VirtualAddr - AddressOfLowestPHDR
INIT
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
	
	uintptr_t Permissions = 0;
	if (~Phdr->Flags & ELF_PHDR_EXEC)
		Permissions |= MM_PTE_NOEXEC;
	
	Permissions |= MM_PTE_READWRITE;
	
	HPAGEMAP PageMap = MiGetCurrentPageMap();
	
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
		PMMPTE Pte = MiGetPTEPointer(PageMap, VirtAddr, false);
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
		
		if (!MiMapPhysicalPage(PageMap,
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

INIT
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

INIT
static PELF_PROGRAM_HEADER LdrpLoadProgramHeaders(PLIMINE_FILE File, uintptr_t *LoadBase, size_t *LoadSize)
{
	PELF_HEADER Header = (PELF_HEADER) File->address;
	
	PELF_PROGRAM_HEADER Dynamic = NULL; // needed later
	
	uintptr_t Offset = (uintptr_t) File->address + Header->ProgramHeadersOffset;
	
	// Scan the program headers to determine the size of our memory region.
	uintptr_t BaseAddr, LargestAddr;
	LdrpGetLimits(File, &BaseAddr, &LargestAddr);
	
	if (BaseAddr > LargestAddr)
		KeCrashBeforeSMPInit("LdrpLoadProgramHeaders: Error, base address %p > largest address %p", BaseAddr, LargestAddr);
	
	uintptr_t Size = LargestAddr - BaseAddr;
	
	// Allocate this many pages.
	Size = (Size + PAGE_SIZE - 1) / PAGE_SIZE;
	if (Size == 0)
		KeCrashBeforeSMPInit("LdrpLoadProgramHeaders: Error, size is zero");
	
	uintptr_t VirtualAddr = LdrAllocateRange(Size);
	*LoadBase = VirtualAddr;
	*LoadSize = Size * PAGE_SIZE;
	
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

INIT
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
	
	if (Header->Type != ELF_TYPE_DYNAMIC)
		KeCrashBeforeSMPInit("LdriLoadDll: %s: Error, module is not actually a DLL", File->path);
	
	// Load the program headers into memory.
	uintptr_t LoadBase;
	size_t LoadSize;
	PELF_PROGRAM_HEADER Dynamic = LdrpLoadProgramHeaders(File, &LoadBase, &LoadSize);
	
	LoadedDLL->ImageBase = LoadBase;
	LoadedDLL->ImageSize = LoadSize;
	
	if (!Dynamic)
		KeCrashBeforeSMPInit("LdriLoadDll: %s: No dynamic table?", File->path);
	
	ELF_DYNAMIC_INFO DynInfo;
	PELF_DYNAMIC_ITEM DynTable = (PELF_DYNAMIC_ITEM) (File->address + Dynamic->Offset);
	if (!RtlParseDynamicTable(DynTable, &DynInfo, LoadBase))
		KeCrashBeforeSMPInit("LdriLoadDll: %s: Failed to parse dynamic table", File->path);
	
	if (!RtlPerformRelocations(&DynInfo, LoadBase))
		KeCrashBeforeSMPInit("LdriLoadDll: %s: Failed to perform relocations", File->path);
	
	RtlFindSymbolTable(File->address, &DynInfo);
	RtlRelocateRelrEntries(&DynInfo, LoadBase);
	
	if (!RtlLinkPlt(&DynInfo, LoadBase, true, File->path))
		KeCrashBeforeSMPInit("LdriLoadDll: %s: Failed to link with the kernel", File->path);
	
	LoadedDLL->EntryPoint = (PDLL_ENTRY_POINT)(LoadBase + (uintptr_t) Header->EntryPoint);
	
	if (!LoadedDLL->EntryPoint)
		KeCrashBeforeSMPInit("LdriLoadDll: %s: Unable to find DriverEntry", File->path);
	
	LoadedDLL->StringTable     = DynInfo.StringTable;
	LoadedDLL->SymbolTable     = DynInfo.SymbolTable;
	LoadedDLL->SymbolTableSize = DynInfo.SymbolTableSize;
	
	DbgPrint("Module %s was loaded at base %p", File->path, LoadBase);
}

const char* LdrLookUpRoutineNameByAddress(PLOADED_DLL LoadedDll, uintptr_t Address, uintptr_t* BaseAddress)
{
	if (!LoadedDll->StringTable || !LoadedDll->SymbolTable)
		return NULL;
	
	const uintptr_t No = (uintptr_t)-1;
	
	uintptr_t NameOffset = No;
	uintptr_t ClosestAddress = 0;
	
	size_t SymbolTableSize  = LoadedDll->SymbolTableSize;
	PELF_SYMBOL SymbolTable = LoadedDll->SymbolTable;
	
	for (size_t i = 0; i < SymbolTableSize; i++)
	{
		uintptr_t SymAddr = SymbolTable[i].Value;
		
		if (ClosestAddress < SymAddr && SymAddr <= Address)
			ClosestAddress = SymAddr, NameOffset = SymbolTable[i].Name;
	}
	
	if (NameOffset == No)
		return NULL;
	
	*BaseAddress = ClosestAddress;
	return LoadedDll->StringTable + NameOffset;
}

INIT
static size_t LdrpGetBareNameLength(const char* Name)
{
	size_t Length = strlen(Name);
	size_t LengthCopy = Length;
	
	for (; Length != 0; Length--)
	{
		if (strcmp(Name + Length, ".sys") == 0)
			return Length;
	}
	
	// Just don't do it.
	return LengthCopy;
}

INIT
static void LdrpInitializeDllByIndex(PLOADED_DLL Dll)
{
	// Create a driver object.
	BSTATUS Status;
	void* ObjectPtr = NULL;
	
	// The first DLL to be loaded is the HAL. Therefore, check if this
	// matches with KeLoadedDLLs[0].
	if (Dll == &KeLoadedDLLs[0])
	{
		// Yes, this is the HAL.
		ObjectPtr = &LdrpHalReservedDriverObject;
	}
	else
	{
		// Use the object manager to create a driver object.
		
		// Take off the ".sys" from the driver's name.
		char DriverName[16];
		size_t Length = LdrpGetBareNameLength(Dll->Name) + 1;
		if (Length > 16)
			Length = 16;
		
		StringCopySafe(DriverName, Dll->Name, Length);
		
		DbgPrint("Creating driver object at \\Drivers\\%s", DriverName);
		Status = ObCreateObject(
			&ObjectPtr,
			IoDriversDir,
			IoDriverType,
			DriverName,
			OB_FLAG_KERNEL | OB_FLAG_NONPAGED | OB_FLAG_PERMANENT, // <-- TODO: Unloadable drivers
			NULL,
			sizeof(DRIVER_OBJECT)
		);
		
		if (FAILED(Status))
			KeCrash("Could not create driver object at \\Drivers\\%s: error %d", DriverName, Status);
	}
	
	PDRIVER_OBJECT Driver = ObjectPtr;
	Dll->DriverObject = Driver;
	
	// Initialize the driver object with basic details.
	// The driver itself will fill in the rest.
	memset(Driver, 0, sizeof * Driver);
	Driver->DriverName = Dll->Name;
	Driver->DriverEntry = Dll->EntryPoint;
	InitializeListHead(&Driver->DeviceList);
	
	// Call the entry point now.
	BSTATUS Result = Dll->EntryPoint(Driver);
	if (Result != 0)
		// XXX: Maybe should be an error instead?
		LogMsg(ANSI_YELLOW "Warning" ANSI_DEFAULT ": Driver %s returned %d at init", Dll->Name, Result);
}

INIT
void LdrInitializeHal()
{
	LdrpInitializeDllByIndex(&KeLoadedDLLs[0]);
}

INIT
void LdrInitializeDrivers()
{
	for (int i = 1; i < KeLoadedDLLCount; i++)
	{
		LdrpInitializeDllByIndex(&KeLoadedDLLs[i]);
	}
}
