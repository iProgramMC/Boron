/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/pmm.c
	
Abstract:
	This module contains the implementation for the physical
	memory manager in Boron.
	
Author:
	iProgramInCpp - 28 August 2023
***/

#include "mi.h"

// TODO: An 'int' PFN is sufficient for now! It allows up to 16 TB
// of physical memory to be represented right now, which is 1024 times
// what's in my computer

extern volatile struct limine_hhdm_request   KeLimineHhdmRequest;
extern volatile struct limine_memmap_request KeLimineMemMapRequest;

size_t MmTotalAvailablePages;

uint8_t* MmGetHHDMBase()
{
	return (uint8_t*)KeLimineHhdmRequest.response->offset;
}

void* MmGetHHDMOffsetAddr(uintptr_t physAddr)
{
	return (void*) (MmGetHHDMBase() + physAddr);
}

uintptr_t MmGetHHDMOffsetFromAddr(void* addr)
{
	return (uintptr_t) addr - (uintptr_t) MmGetHHDMBase();
}

// Allocates a page from the memmap for eternity during init.  Used to prepare the PFN database.
// Also used in the initial DLL loader.
uintptr_t MiAllocatePageFromMemMap()
{
	struct limine_memmap_response* pResponse = KeLimineMemMapRequest.response;
	
	for (uint64_t i = 0; i < pResponse->entry_count; i++)
	{
		// if the entry isn't usable, skip it
		struct limine_memmap_entry* pEntry = pResponse->entries[i];
		
		if (pEntry->type != LIMINE_MEMMAP_USABLE)
			continue;
		
		// Note! Usable entries in limine are guaranteed to be aligned to
		// page size, and not overlap any other entries. So we are good
		
		// if it's got no pages, also skip it..
		if (pEntry->length == 0)
			continue;
		
		uintptr_t curr_addr = pEntry->base;
		pEntry->base   += PAGE_SIZE;
		pEntry->length -= PAGE_SIZE;
		
		return curr_addr;
	}
	
	KeCrashBeforeSMPInit("Error, out of memory in the memmap allocate function");
}

// Allocate a contiguous area of memory from the memory map.
// Used only by the terminal.
uintptr_t MiAllocateMemoryFromMemMap(size_t SizeInPages)
{
	size_t Size = SizeInPages * PAGE_SIZE;
	
	struct limine_memmap_response* pResponse = KeLimineMemMapRequest.response;
	
	for (uint64_t i = 0; i < pResponse->entry_count; i++)
	{
		// if the entry isn't usable, skip it
		struct limine_memmap_entry* pEntry = pResponse->entries[i];
		
		if (pEntry->type != LIMINE_MEMMAP_USABLE)
			continue;
		
		// Note! Usable entries in limine are guaranteed to be aligned to
		// page size, and not overlap any other entries. So we are good
		
		// if it's got no pages, also skip it..
		if (pEntry->length < Size)
			continue;
		
		uintptr_t curr_addr = pEntry->base;
		pEntry->base   += Size;
		pEntry->length -= Size;
		
		return curr_addr;
	}
	
	KeCrashBeforeSMPInit("Error, out of memory in the memmap allocate function");
}

static bool MiMapNewPageAtAddressIfNeeded(uintptr_t pageTable, uintptr_t address)
{
#ifdef TARGET_AMD64
	// Maps a new page at an address, if needed.
	PageMapLevel *pPML[4];
	pPML[3] = (PageMapLevel*) MmGetHHDMOffsetAddr(pageTable);
	
	for (int i = 3; i >= 0; i--)
	{
		int index = (address >> (12 + 9 * i)) & 0x1FF;
		if (pPML[i]->entries[index] & MM_PTE_PRESENT)
		{
			if (i == 0)
				return true; // didn't allocate anything
			
			pPML[i - 1] = (PageMapLevel*) MmGetHHDMOffsetAddr(PTE_ADDRESS(pPML[i]->entries[index]));
		}
		else
		{
			uintptr_t Addr = MiAllocatePageFromMemMap();
			
			if (!Addr)
			{
				// TODO: Allow rollback
				return false;
			}
			
			uint64_t Flags = MM_PTE_PRESENT | MM_PTE_READWRITE | MM_PTE_SUPERVISOR | MM_PTE_NOEXEC;
			
			if (i != 0)
				pPML[i - 1] = (PageMapLevel*) MmGetHHDMOffsetAddr(Addr);
			else
				Flags |= MM_PTE_GLOBAL;
			
			pPML[i]->entries[index] = Addr | Flags;
		}
	}
	
	return true;
#else
	#error "Implement this for your platform!"
#endif
}

int MmPhysPageToPFN(uintptr_t physAddr)
{
	return physAddr / PAGE_SIZE;
}

uintptr_t MmPFNToPhysPage(int pfn)
{
	return (uintptr_t) pfn * PAGE_SIZE;
}

MMPFN* MmGetPageFrameFromPFN(int pfn)
{
	MMPFN* pPFNDB = (MMPFN*) MM_PFNDB_BASE;
	
	return &pPFNDB[pfn];
}

void MmZeroOutFirstPFN();

// Two lists: a list of "ZERO" pfns and a list of "FREE" pfns.
// The zeroed PFN list will be prioritised for speed. If there
// are no free zero pfns, then a free PFN will be zeroed before
// being issued.
static int MiFirstZeroPFN = PFN_INVALID, MiLastZeroPFN = PFN_INVALID;
static int MiFirstFreePFN = PFN_INVALID, MiLastFreePFN = PFN_INVALID;
static KSPIN_LOCK MmPfnLock;

// Note! Initialization is done on the BSP. So no locking needed
void MiInitPMM()
{
	if (!KeLimineMemMapRequest.response || !KeLimineHhdmRequest.response)
	{
		KeCrashBeforeSMPInit("Error, no memory map was provided");
	}
	
	// with 4-level paging, limine seems to be hardcoded at this address, so we're probably good. Although
	// the protocol does NOT specify that, and it does seem to be affected by KASLR...
	if (KeLimineHhdmRequest.response->offset != 0xFFFF800000000000)
	{
		KeCrashBeforeSMPInit("The HHDM isn't at 0xFFFF 8000 0000 0000, things may go wrong! (It's actually at %p)", (void*) KeLimineHhdmRequest.response->offset);
	}
	
	uintptr_t currPageTablePhys = KeGetCurrentPageTable();
	
	// allocate the entries in the page frame number database
	struct limine_memmap_response* pResponse = KeLimineMemMapRequest.response;
	
	// pass 0: print out all the entries for debugging
	for (uint64_t i = 0; i < pResponse->entry_count; i++)
	{
		// if the entry isn't usable, skip it
		struct limine_memmap_entry* pEntry = pResponse->entries[i];
		
		if (pEntry->type != LIMINE_MEMMAP_USABLE)
			continue;
		
		DbgPrint("%p-%p (%d pages)", pEntry->base, pEntry->base + pEntry->length, pEntry->length / PAGE_SIZE);
	}
	
	// pass 1: mapping the pages themselves
	int numAllocatedPages = 0;
	for (uint64_t i = 0; i < pResponse->entry_count; i++)
	{
		// if the entry isn't usable, skip it
		struct limine_memmap_entry *pEntry = pResponse->entries[i];
		
		if (pEntry->type != LIMINE_MEMMAP_USABLE)
			continue;
		
		// make a copy since we might tamper with this
		struct limine_memmap_entry entry = *pEntry;
		
		int pfnStart = MmPhysPageToPFN(entry.base);
		int pfnEnd   = MmPhysPageToPFN(entry.base + entry.length);
		
		uint64_t lastAllocatedPage = 0;
		for (int pfn = pfnStart; pfn < pfnEnd; pfn++)
		{
			uint64_t currPage = (MM_PFNDB_BASE + sizeof(MMPFN) * pfn) & ~(PAGE_SIZE - 1);
			if (lastAllocatedPage == currPage) // check is probably useless
				continue;
			
			if (!MiMapNewPageAtAddressIfNeeded(currPageTablePhys, currPage))
				KeCrashBeforeSMPInit("Error, couldn't setup PFN database");
			
			lastAllocatedPage = currPage;
			numAllocatedPages++;
		}
	}
	
	DbgPrint("Initializing the PFN database.", sizeof(MMPFN));
	// pass 2: Initting the PFN database
	int lastPfnOfPrevBlock = PFN_INVALID;
	
	for (uint64_t i = 0; i < pResponse->entry_count; i++)
	{
		// if the entry isn't usable, skip it
		struct limine_memmap_entry* pEntry = pResponse->entries[i];
		
		if (pEntry->type != LIMINE_MEMMAP_USABLE)
			continue;
		
		for (uint64_t j = 0; j < pEntry->length; j += PAGE_SIZE)
		{
			int currPFN = MmPhysPageToPFN(pEntry->base + j);
			
			if (MiFirstFreePFN == PFN_INVALID)
				MiFirstFreePFN =  currPFN;
			
			MMPFN* pPF = MmGetPageFrameFromPFN(currPFN);
			
			// initialize this PFN
			memset(pPF, 0, sizeof *pPF);
			
			if (j == 0)
			{
				pPF->PrevFrame = lastPfnOfPrevBlock;
				
				// also update the last PF's next frame idx
				if (lastPfnOfPrevBlock != PFN_INVALID)
				{
					MMPFN* pPrevPF = MmGetPageFrameFromPFN(lastPfnOfPrevBlock);
					pPrevPF->NextFrame = currPFN;
				}
			}
			else
			{
				pPF->PrevFrame = currPFN - 1;
			}
			
			if (j + PAGE_SIZE >= pEntry->length)
			{
				pPF->NextFrame = PFN_INVALID; // it's going to be updated by the next block if there's one
			}
			else
			{
				pPF->NextFrame = currPFN + 1;
			}
			
			lastPfnOfPrevBlock = currPFN;
		}
	}
	
	MiLastFreePFN = lastPfnOfPrevBlock;
	
	// zero out like 200 of these
	for (int i = 0; i < 200; i++)
		MmZeroOutFirstPFN();
	
	DbgPrint("PFN database initialized.  Reserved %d pages (%d KB)", numAllocatedPages, numAllocatedPages * PAGE_SIZE / 1024);
	
	// final evaluation of the current amount of memory:
	size_t TotalMemory = 0;
	for (uint64_t i = 0; i < pResponse->entry_count; i++)
	{
		// if the entry isn't usable, skip it
		struct limine_memmap_entry* pEntry = pResponse->entries[i];
		
		if (pEntry->type != LIMINE_MEMMAP_USABLE)
			continue;
		
		TotalMemory += pEntry->length;
	}
	
	DbgPrint("MiInitPMM: %zu Kb Available Memory", TotalMemory / 1024);
	
	MmTotalAvailablePages = TotalMemory / PAGE_SIZE;
}

// TODO: Add locking

static void MmpRemovePfnFromList(int* First, int* Last, int Current)
{
	MMPFN *pPF = MmGetPageFrameFromPFN(Current);
	
	// disconnect its neighbors from this one
	if (pPF->NextFrame != PFN_INVALID)
		MmGetPageFrameFromPFN(pPF->NextFrame)->PrevFrame = pPF->PrevFrame;
	if (pPF->PrevFrame != PFN_INVALID)
		MmGetPageFrameFromPFN(pPF->PrevFrame)->NextFrame = pPF->NextFrame;
	
	if (*First == Current)
	{
		// set the first PFN of the list to the next PFN
		*First = pPF->NextFrame;
		
		// if the next frame is PFN_INVALID, we're done with the list entirely...
		if (*First == PFN_INVALID)
			*Last  =  PFN_INVALID;
	}
	if (*Last == Current)
	{
		// set the last PFN of the list to the prev PFN
		*Last = pPF->PrevFrame;
		
		// if the prev frame is PFN_INVALID, we're done with the list entirely...
		if (*Last  == PFN_INVALID)
			*First =  PFN_INVALID;
	}
}

static void MmpAddPfnToList(int* First, int* Last, int Current)
{
	if (*First == PFN_INVALID)
	{
		*First = *Last = Current;
		
		MMPFN* pPF = MmGetPageFrameFromPFN(Current);
		pPF->NextFrame = PFN_INVALID;
		pPF->PrevFrame = PFN_INVALID;
	}
	
	MMPFN *pLastPFN = MmGetPageFrameFromPFN(*Last), *pCurrPFN = MmGetPageFrameFromPFN(Current);
	
	pLastPFN->NextFrame = Current;
	pCurrPFN->PrevFrame = *Last;
	pCurrPFN->NextFrame = PFN_INVALID;
	// type will be updated by caller
	*Last = Current;
}

static int MmpAllocateFromFreeList(int* First, int* Last)
{
	if (*First == PFN_INVALID)
		return PFN_INVALID;
	
	int currPFN = *First;
	MMPFN *pPF = MmGetPageFrameFromPFN(*First);
	
	pPF->Type = PF_TYPE_USED;
	MmpRemovePfnFromList(First, Last, *First);
	
	return currPFN;
}

int MmAllocatePhysicalPage()
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	int currPFN = MmpAllocateFromFreeList(&MiFirstZeroPFN, &MiLastZeroPFN);
	if (currPFN == PFN_INVALID)
		currPFN = MmpAllocateFromFreeList(&MiFirstFreePFN, &MiLastFreePFN);
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
	
#ifdef DEBUG2
	DbgPrint("MmAllocatePhysicalPage() => %d (RA:%p)", currPFN, __builtin_return_address(0));
#endif
	
	return currPFN;
}

void MmFreePhysicalPage(int pfn)
{
	KIPL OldIpl;
	
#ifdef DEBUG2
	DbgPrint("MmFreePhysicalPage()     <= %d (RA:%p)", pfn, __builtin_return_address(0));
#endif
	
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	MmpAddPfnToList(&MiFirstFreePFN, &MiLastFreePFN, pfn);
	MmGetPageFrameFromPFN(pfn)->Type = PF_TYPE_FREE;
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

// Zeroes out a free PFN, takes it off the free PFN list and adds it to
// the zero PFN list.
static void MmpZeroOutPFN(int pfn)
{
	MMPFN* pPF = MmGetPageFrameFromPFN(pfn);
	if (pPF->Type == PF_TYPE_ZEROED)
		return;
	
	if (pPF->Type != PF_TYPE_FREE)
	{
		DbgPrint("Error, attempting to zero out pfn %d which is used", pfn);
		return;
	}
	
	pPF->Type = PF_TYPE_ZEROED;
	
	MmpRemovePfnFromList(&MiFirstFreePFN, &MiLastFreePFN, pfn);
	
	// zero out the page itself
	uint8_t* mem = MmGetHHDMOffsetAddr(MmPFNToPhysPage(pfn));
	memset(mem, 0, PAGE_SIZE);
	
	MmpAddPfnToList(&MiFirstZeroPFN, &MiLastZeroPFN, pfn);
}

void MmZeroOutPFN(int pfn)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	MmpZeroOutPFN(pfn);
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

void MmZeroOutFirstPFN()
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	if (MiFirstFreePFN == PFN_INVALID)
	{
		KeReleaseSpinLock(&MmPfnLock, OldIpl);
		return;
	}
	
	MmpZeroOutPFN(MiFirstFreePFN);
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

void* MmAllocatePhysicalPageHHDM()
{
	int PhysicalPage = MmAllocatePhysicalPage();
	
	if (PhysicalPage == PFN_INVALID)
		return NULL;
	
	return MmGetHHDMOffsetAddr(MmPFNToPhysPage(PhysicalPage));
}

void MmFreePhysicalPageHHDM(void* page)
{
	return MmFreePhysicalPage(MmPhysPageToPFN(MmGetHHDMOffsetFromAddr(page)));
}
