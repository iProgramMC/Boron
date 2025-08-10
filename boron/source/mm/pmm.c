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

#ifdef DEBUG2
#define PMMDEBUG
#endif

extern volatile struct limine_hhdm_request   KeLimineHhdmRequest;
extern volatile struct limine_memmap_request KeLimineMemMapRequest;

size_t MmTotalAvailablePages;
size_t MmTotalFreePages;

#ifdef DEBUG
size_t MmReclaimedPageCount;
#endif

uintptr_t MmHHDMBase;

size_t MmGetTotalFreePages()
{
	return MmTotalFreePages;
}

uint8_t* MmGetHHDMBase()
{
	return (uint8_t*)MmHHDMBase;
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
INIT
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
INIT
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

typedef struct
{
	uint64_t entries[512];
}
PageMapLevel;

#define PTE_ADDRESS(pte) ((pte) & MM_PTE_ADDRESSMASK)

INIT
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
			
			memset(MmGetHHDMOffsetAddr(Addr), 0, PAGE_SIZE);
			
			uint64_t Flags = MM_PTE_PRESENT | MM_PTE_READWRITE | MM_PTE_NOEXEC;
			
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

MMPFN MmPhysPageToPFN(uintptr_t PhysAddr)
{
	return (MMPFN) (PhysAddr / PAGE_SIZE);
}

uintptr_t MmPFNToPhysPage(MMPFN Pfn)
{
	return (uintptr_t) Pfn * PAGE_SIZE;
}

MMPFDBE* MmGetPageFrameFromPFN(MMPFN Pfn)
{
	PMMPFDBE pPFNDB = (PMMPFDBE) MM_PFNDB_BASE;
	
	return &pPFNDB[Pfn];
}

void MmZeroOutFirstPFN();

// Three lists:
// - a list of "zero" PFs which contain only pages that have been
//   zeroed out
// - a list of "free" PFs which have no references at all to them and
// - a list of "standby" PFs, whose reference count is zero but they
//   are still referenced in a cache control block (io/cache).  When
//   a page is removed from the standby list, its prototype PTE will
//   be zeroed out atomically (the prototype PTE, in the case of stand-
//   by pages, is the pointer to the entry in the CCB)
//
// The zeroed PFN list will be prioritised for speed. If there
// are no free zero pfns, then a free PFN will be zeroed before
// being issued.
//
// If both the zero and free PFN lists are invalid, a page is withdrawn
// from the standby list (also if available).
//
// The modified list is NOT a list of free PFNs.  However, the
// modified page writer will continuously write pages and turn them
// into standby pages.
static MMPFN MiFirstZeroPFN = PFN_INVALID, MiLastZeroPFN = PFN_INVALID;
static MMPFN MiFirstFreePFN = PFN_INVALID, MiLastFreePFN = PFN_INVALID;
static MMPFN MiFirstStandbyPFN = PFN_INVALID, MiLastStandbyPFN = PFN_INVALID;
static MMPFN MiFirstModifiedPFN = PFN_INVALID, MiLastModifiedPFN = PFN_INVALID;
static KSPIN_LOCK MmPfnLock;

KIPL MiLockPfdb()
{
	KIPL Ipl;
	KeAcquireSpinLock(&MmPfnLock, &Ipl);
	return Ipl;
}

void MiUnlockPfdb(KIPL Ipl)
{
	KeReleaseSpinLock(&MmPfnLock, Ipl);
}

// Note! Initialization is done on the BSP. So no locking needed
INIT
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
	
	MmHHDMBase = KeLimineHhdmRequest.response->offset;
	
	uintptr_t currPageTablePhys = KeGetCurrentPageTable();
	
	// allocate the entries in the page frame number database
	struct limine_memmap_response* pResponse = KeLimineMemMapRequest.response;
	
	// pass 0: print out all the entries for debugging
	for (uint64_t i = 0; i < pResponse->entry_count; i++)
	{
		// if the entry isn't usable, skip it
		struct limine_memmap_entry* pEntry = pResponse->entries[i];
		
		if (pEntry->type != LIMINE_MEMMAP_USABLE &&
			pEntry->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE &&
			pEntry->type != LIMINE_MEMMAP_KERNEL_AND_MODULES)
			continue;
		
		DbgPrint("%p-%p (%d pages, %d)", pEntry->base, pEntry->base + pEntry->length, pEntry->length / PAGE_SIZE, pEntry->type);
	}
	
	// pass 1: mapping the pages themselves
	int numAllocatedPages = 0;
	for (uint64_t i = 0; i < pResponse->entry_count; i++)
	{
		// if the entry isn't usable, skip it
		struct limine_memmap_entry *pEntry = pResponse->entries[i];
		
		if (pEntry->type != LIMINE_MEMMAP_USABLE && pEntry->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE)
			continue;
		
		// make a copy since we might tamper with this
		struct limine_memmap_entry entry = *pEntry;
		
		int pfnStart = MmPhysPageToPFN(entry.base);
		int pfnEnd   = MmPhysPageToPFN(entry.base + entry.length);
		
		uint64_t lastAllocatedPage = 0;
		for (int pfn = pfnStart; pfn < pfnEnd; pfn++)
		{
			uint64_t currPage = (MM_PFNDB_BASE + sizeof(MMPFDBE) * pfn) & ~(PAGE_SIZE - 1);
			if (lastAllocatedPage == currPage) // check is probably useless
				continue;
			
			if (!MiMapNewPageAtAddressIfNeeded(currPageTablePhys, currPage))
				KeCrashBeforeSMPInit("Error, couldn't setup PFN database");
			
			lastAllocatedPage = currPage;
			numAllocatedPages++;
		}
	}
	
	DbgPrint("Initializing the PFN database.", sizeof(MMPFDBE));
	// pass 2: Initting the PFN database
	int lastPfnOfPrevBlock = PFN_INVALID;
	
	int TotalPageCount = 0, FreePageCount = 0, ReclaimPageCount = 0;
	
	for (uint64_t i = 0; i < pResponse->entry_count; i++)
	{
		// if the entry isn't usable, skip it
		struct limine_memmap_entry* pEntry = pResponse->entries[i];
		
		if (pEntry->type != LIMINE_MEMMAP_USABLE &&
			pEntry->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE &&
			pEntry->type != LIMINE_MEMMAP_KERNEL_AND_MODULES)
			continue;
		
		for (uint64_t j = 0; j < pEntry->length; j += PAGE_SIZE)
		{
			bool isUsed = pEntry->type != LIMINE_MEMMAP_USABLE;
			
			int currPFN = MmPhysPageToPFN(pEntry->base + j);
			
			TotalPageCount++;
			
			if (isUsed)
			{
				// This is used but reclaimable memory.  This means we should make its PFDBE used.
				PMMPFDBE pPF = MmGetPageFrameFromPFN(currPFN);
				
				memset(pPF, 0, sizeof *pPF);
				
				pPF->Type = PF_TYPE_RECLAIM;
				pPF->RefCount = 1;
				
				ReclaimPageCount++;
			}
			else
			{
				if (MiFirstFreePFN == PFN_INVALID)
					MiFirstFreePFN =  currPFN;
				
				PMMPFDBE pPF = MmGetPageFrameFromPFN(currPFN);
				
				// initialize this PFN
				memset(pPF, 0, sizeof *pPF);
				
				if (j == 0)
				{
					pPF->PrevFrame = lastPfnOfPrevBlock;
					
					// also update the last PF's next frame idx
					if (lastPfnOfPrevBlock != PFN_INVALID)
					{
						MMPFDBE* pPrevPF = MmGetPageFrameFromPFN(lastPfnOfPrevBlock);
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
				
				FreePageCount++;
			}
		}
	}
	
	MiLastFreePFN = lastPfnOfPrevBlock;
	
	// zero out like 200 of these
	for (int i = 0; i < 200; i++)
		MmZeroOutFirstPFN();
	
	DbgPrint("PFN database initialized.  Reserved %d pages (%d KB)", numAllocatedPages, numAllocatedPages * PAGE_SIZE / 1024);
	
	MmTotalFreePages = FreePageCount;
	MmTotalAvailablePages = TotalPageCount;
	
	DbgPrint("MiInitPMM: %zu Kb Available Memory", MmTotalAvailablePages * PAGE_SIZE / 1024);
}

// This is split into two parts to allow for use in a function other than MmpRemovePfnFromList
FORCE_INLINE
void MmpUnlinkPfn(PMMPFDBE Pfdbe)
{
	// disconnect its neighbors from this one
	if (Pfdbe->NextFrame != PFN_INVALID)
		MmGetPageFrameFromPFN(Pfdbe->NextFrame)->PrevFrame = Pfdbe->PrevFrame;
	if (Pfdbe->PrevFrame != PFN_INVALID)
		MmGetPageFrameFromPFN(Pfdbe->PrevFrame)->NextFrame = Pfdbe->NextFrame;
}

FORCE_INLINE
void MmpEnsurePfnIsntEndsOfList(PMMPFN First, PMMPFN Last, PMMPFDBE Pfdbe, MMPFN Current)
{
	if (*First == Current)
	{
		// set the first PFN of the list to the next PFN
		*First = Pfdbe->NextFrame;
		
		// if the next frame is PFN_INVALID, we're done with the list entirely...
		if (*First == PFN_INVALID)
			*Last  =  PFN_INVALID;
	}
	if (*Last == Current)
	{
		// set the last PFN of the list to the prev PFN
		*Last = Pfdbe->PrevFrame;
		
		// if the prev frame is PFN_INVALID, we're done with the list entirely...
		if (*Last  == PFN_INVALID)
			*First =  PFN_INVALID;
	}
}

static void MmpRemovePfnFromList(PMMPFN First, PMMPFN Last, MMPFN Current)
{
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Current);
	MmpUnlinkPfn(Pfdbe);
	MmpEnsurePfnIsntEndsOfList(First, Last, Pfdbe, Current);
}

void MiDetransitionPfn(MMPFN Pfn)
{
	ASSERT(MmPfnLock.Locked);
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	
	ASSERT(Pfdbe->FileCache._PrototypePte && "How did we recover this page?!");
	
	MmpUnlinkPfn(Pfdbe);
	MmpEnsurePfnIsntEndsOfList(&MiFirstStandbyPFN, &MiLastStandbyPFN, Pfdbe, Pfn);
	MmpEnsurePfnIsntEndsOfList(&MiFirstModifiedPFN, &MiLastModifiedPFN, Pfdbe, Pfn);
	
	// Now that the PFN is unlinked, we can turn it into a used PFN.
	Pfdbe->FileCache._PrototypePte = 0;
	Pfdbe->RefCount = 1;
	Pfdbe->NextFrame = 0;
	Pfdbe->PrevFrame = 0;
	Pfdbe->Type = PF_TYPE_USED;
}

static void MmpAddPfnToList(PMMPFN First, PMMPFN Last, MMPFN Current)
{
	if (*First == PFN_INVALID)
	{
		*First = *Last = Current;
		
		MMPFDBE* pPF = MmGetPageFrameFromPFN(Current);
		pPF->NextFrame = PFN_INVALID;
		pPF->PrevFrame = PFN_INVALID;
	}
	
	PMMPFDBE pLastPFN = MmGetPageFrameFromPFN(*Last), pCurrPFN = MmGetPageFrameFromPFN(Current);
	
	pLastPFN->NextFrame = Current;
	pCurrPFN->PrevFrame = *Last;
	pCurrPFN->NextFrame = PFN_INVALID;
	// type will be updated by caller
	*Last = Current;
}

static void MmpInitializePfn(PMMPFDBE Pfdbe)
{
	ASSERT(MmPfnLock.Locked);
	
	if (Pfdbe->Type == PF_TYPE_USED)
		return;
	
	if (Pfdbe->Type == PF_TYPE_TRANSITION)
	{
		// This is a transition page.  Zero out what's at the prototype PTE's address to reclaim.
		//
		// If any code wants to modify a transitional PTE, they must acquire the PFDB lock to ensure
		// that it isn't reclaimed by this part of the code.
		//
		// Something like this to de-transition a PTE:
		//   pte - The PTE in question
		//   if (pte & TRANSITION) {
		//       acquire PTE lock
		//       if (pte == 0) {   // awe damn we missed it
		//           release PTE lock
		//           // gotta fault it back in...
		//       }
		//       MiDetransitionPfn(PFN(pte))
		//       release PTE lock
		//   }
		ASSERT(Pfdbe->FileCache._PrototypePte);
		
		uintptr_t* Ptr = PFDBE_PrototypePte(Pfdbe);
		AtStore(*Ptr, 0);
	}
	
	Pfdbe->Type = PF_TYPE_USED;
	Pfdbe->RefCount = 1;
	Pfdbe->NextFrame = 0;
	Pfdbe->PrevFrame = 0;
	Pfdbe->FileCache._PrototypePte = 0;
	MmTotalFreePages--;
}

static MMPFN MmpAllocateFromFreeList(PMMPFN First, PMMPFN Last)
{
	if (*First == PFN_INVALID)
		return PFN_INVALID;
	
	int currPFN = *First;
	PMMPFDBE pPF = MmGetPageFrameFromPFN(*First);
	
	MmpRemovePfnFromList(First, Last, *First);
	MmpInitializePfn(pPF);
	
	return currPFN;
}

MMPFN MmAllocatePhysicalPage()
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	MMPFN currPFN = MmpAllocateFromFreeList(&MiFirstZeroPFN, &MiLastZeroPFN);
	if (currPFN == PFN_INVALID)
		currPFN = MmpAllocateFromFreeList(&MiFirstFreePFN, &MiLastFreePFN);
	if (currPFN == PFN_INVALID)
		currPFN = MmpAllocateFromFreeList(&MiFirstStandbyPFN, &MiLastStandbyPFN);
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
	
#ifdef PMMDEBUG
	DbgPrint("MmAllocatePhysicalPage() => %d (RA:%p)", currPFN, __builtin_return_address(0));
#endif
	
	return currPFN;
}

void MmSetPrototypePtePfn(MMPFN Pfn, uintptr_t* PrototypePte)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	// If this page is freed after this operation, then the prototype
	// PTE will be atomically set to zero when reclaimed.
	MmGetPageFrameFromPFN(Pfn)->FileCache._PrototypePte = (uint64_t) PrototypePte;
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

void MmFreePhysicalPage(MMPFN pfn)
{
	KIPL OldIpl;
	
#ifdef PMMDEBUG
	DbgPrint("MmFreePhysicalPage()     <= %d (RA:%p)", pfn, __builtin_return_address(0));
#endif
	
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	PMMPFDBE PageFrame = MmGetPageFrameFromPFN(pfn);
	
	ASSERT(PageFrame->Type == PF_TYPE_USED || PageFrame->Type == PF_TYPE_RECLAIM);
	
	int RefCount = --PageFrame->RefCount;
	ASSERT(RefCount >= 0);
	
	if (RefCount <= 0)
	{
#ifdef DEBUG
		if (PageFrame->Type == PF_TYPE_RECLAIM)
			MmReclaimedPageCount++;
#endif
		
		if (PageFrame->FileCache._PrototypePte)
		{
			// This is part of a cache control block.  Was this written?
			if (PageFrame->Modified)
			{
				// Yes, we should add it to the modified list. However, this
				// page will remain marked as "used" until the modified page
				// writer actually writes to the page.
				//
				// TODO: Signal the modified page writter to start writing.
				MmpAddPfnToList(&MiFirstModifiedPFN, &MiLastModifiedPFN, pfn);
			}
			else
			{
				MmpAddPfnToList(&MiFirstStandbyPFN, &MiLastStandbyPFN, pfn);
				PageFrame->Type = PF_TYPE_TRANSITION;
				MmTotalFreePages++;
			}
		}
		else
		{
			// This page is completely free.
			MmpAddPfnToList(&MiFirstFreePFN, &MiLastFreePFN, pfn);
			PageFrame->Type = PF_TYPE_FREE;
			MmTotalFreePages++;
		}
	}
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

// Zeroes out a free PFN, takes it off the free PFN list and adds it to
// the zero PFN list.
static void MmpZeroOutPFN(MMPFN pfn)
{
	PMMPFDBE pPF = MmGetPageFrameFromPFN(pfn);
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

void MmZeroOutPFN(MMPFN pfn)
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
	MMPFN PhysicalPage = MmAllocatePhysicalPage();
	
	if (PhysicalPage == PFN_INVALID)
		return NULL;
	
	return MmGetHHDMOffsetAddr(MmPFNToPhysPage(PhysicalPage));
}

void MmFreePhysicalPageHHDM(void* page)
{
	return MmFreePhysicalPage(MmPhysPageToPFN(MmGetHHDMOffsetFromAddr(page)));
}

void MiPageAddReferenceWithPfdbLocked(MMPFN Pfn)
{
	ASSERT(MmPfnLock.Locked);
	
	PMMPFDBE PageFrame = MmGetPageFrameFromPFN(Pfn);
	PageFrame->RefCount++;
	
	// If its reference count is 1, then we should remove it from any
	// free lists and initialize it as if it were reallocated.
	if (PageFrame->RefCount <= 1)
	{
		// (if it's less than 1 now then it was negative before)
		ASSERT(PageFrame->RefCount == 1);
		MiDetransitionPfn(Pfn);
	}
}

void MiSetModifiedPageWithPfdbLocked(MMPFN Pfn)
{
	ASSERT(MmPfnLock.Locked);
	
	PMMPFDBE PageFrame = MmGetPageFrameFromPFN(Pfn);
	PageFrame->Modified = true;
}

void MmPageAddReference(MMPFN Pfn)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	MiPageAddReferenceWithPfdbLocked(Pfn);
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

int MiGetReferenceCountPfn(MMPFN Pfn)
{
	ASSERT(MmPfnLock.Locked);
	
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	if (Pfdbe->Type != PF_TYPE_USED)
		return 0;
	
	return Pfdbe->RefCount;
}
