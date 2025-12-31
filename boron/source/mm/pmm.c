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

#ifdef PMMDEBUG
#define PmmDbgPrint(...) DbgPrint(__VA_ARGS__)
#else
#define PmmDbgPrint(...) do {} while (0)
#endif

// LOCKING:
//
// You might think that I would need to establish a locking
// order between the PFN lock and the HHDM lock. But I won't.
// Why? Well, let's take a look at the two cases:
//
// 64-bit: The HHDM lock doesn't exist. Like, at all. So,
//     MmBeginUsingHHDM and MmEndUsingHHDM are no-ops, and as
//     such, both orderings work.
//
// 32-bit: The HHDM lock exists. However, since we're on a
//     non-SMP system (assert that later), that means that
//     regardless of what lock we grab first, the other is
//     guaranteed to be unlocked.  So there is no locking-
//     inversion-caused deadlock.

#if defined IS_32_BIT && defined CONFIG_SMP
#error You should fix this locking inversion bug! It could result in nasty deadlocks!
#endif

static KSPIN_LOCK MmPfnLock;

// Free page statistics
size_t MmTotalAvailablePages;
size_t MmTotalFreePages;

#ifdef DEBUG
size_t MmReclaimedPageCount;
#endif

size_t MmGetTotalFreePages()
{
	return MmTotalFreePages;
}

// HHDM
#ifdef IS_64_BIT

uintptr_t MmHHDMBase;

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

#else // IS_64_BIT

#ifdef CONFIG_SMP
#error TODO: Add spinlocks or per-core separation!
#endif

uintptr_t MmHHDMWindowBase;
static KSPIN_LOCK MiHHDMWindowLock;
static KIPL MiHHDMWindowIpl;

void MmBeginUsingHHDM()
{
	KIPL Ipl;
	KeAcquireSpinLock(&MiHHDMWindowLock, &Ipl);
	MiHHDMWindowIpl = Ipl;
}

void MmEndUsingHHDM()
{
	ASSERT(MiHHDMWindowLock.Locked);
	KeReleaseSpinLock(&MiHHDMWindowLock, MiHHDMWindowIpl);
}

static void MiUpdateHHDMWindowBase(uintptr_t PhysAddr)
{
	ASSERT(MiHHDMWindowLock.Locked);
	KIPL Ipl = MiLockPfdb();
	
	const int PtesPerLevel = PAGE_SIZE / sizeof(MMPTE);
	PMMPTE Ptes = (PMMPTE)(MI_PML1_LOCATION);
	
	PhysAddr &= MI_FASTMAP_MASK;
	MmHHDMWindowBase = PhysAddr;
	
	// TODO: Optimize this. We don't really need that many pages.
	for (size_t i = 0; i < MI_FASTMAP_SIZE; i += PAGE_SIZE)
	{
		uintptr_t Address = MI_FASTMAP_START + i;
		
		MMADDRESS_CONVERT Convert;
		Convert.Long = Address;
		
		Ptes[Convert.Level2Index * PtesPerLevel + Convert.Level1Index] = MM_PTE_PRESENT | MM_PTE_READWRITE | MM_PTE_NOEXEC | (PhysAddr + i);
		KeInvalidatePage((void*)Address);
	}
	
	MiUnlockPfdb(Ipl);
}

void* MmGetHHDMOffsetAddr(uintptr_t PhysAddr)
{
	//ASSERT(!MmPfnLock.Locked);
	ASSERT(MiHHDMWindowLock.Locked);
	
	if (PhysAddr < MI_IDENTMAP_SIZE)
		return (void*)(MI_IDENTMAP_START + PhysAddr);
	
	if ((PhysAddr & MI_FASTMAP_MASK) != MmHHDMWindowBase)
		MiUpdateHHDMWindowBase(PhysAddr);
	
	return (void*)(MI_FASTMAP_START + (PhysAddr & ~MI_FASTMAP_MASK));
}

uintptr_t MmGetHHDMOffsetFromAddr(void* Addr)
{
	ASSERT(!MmPfnLock.Locked);
	ASSERT(MiHHDMWindowLock.Locked);
	
	uintptr_t AddrInt = (uintptr_t) Addr;
	if (AddrInt >= MI_IDENTMAP_START && AddrInt < MI_IDENTMAP_START + MI_IDENTMAP_SIZE)
		return AddrInt - MI_IDENTMAP_START;
	
	if ((AddrInt & MI_FASTMAP_MASK) != MmHHDMWindowBase)
	{
		DbgPrint("MmGetHHDMOffsetFromAddr: Address %p isn't in the currently selected window!", Addr);
		return 0xFFFFFFFF;
	}

	return MmHHDMWindowBase + (AddrInt & ~MI_FASTMAP_MASK);
}

#endif // IS_64_BIT

// Allocates a page from the memmap for eternity during init.  Used to prepare the PFN database.
// Also used in the initial DLL loader.
INIT
uintptr_t MiAllocatePageFromMemMap()
{
	PLOADER_MEMORY_REGION MemoryRegions = KeLoaderParameterBlock.MemoryRegions;
	size_t MemoryRegionCount = KeLoaderParameterBlock.MemoryRegionCount;
	
	for (size_t i = 0; i < MemoryRegionCount; i++)
	{
		// if the entry isn't usable, skip it
		PLOADER_MEMORY_REGION Entry = &MemoryRegions[i];
		
		if (Entry->Type != LOADER_MEM_FREE)
			continue;
		
		// Note: Usable entries are guaranteed to be aligned to page size,
		// and not overlap any other entries.
		
		// if it's got no pages, also skip it..
		if (Entry->Size == 0)
			continue;
		
		uintptr_t CurrAddr = Entry->Base;
		Entry->Base += PAGE_SIZE;
		Entry->Size -= PAGE_SIZE;
		
		return CurrAddr;
	}
	
	KeCrashBeforeSMPInit("Error, out of memory in the memmap allocate function");
}

// Allocate a contiguous area of memory from the memory map.
INIT
uintptr_t MiAllocateMemoryFromMemMap(size_t SizeInPages)
{
	size_t Size = SizeInPages * PAGE_SIZE;
	
	PLOADER_MEMORY_REGION MemoryRegions = KeLoaderParameterBlock.MemoryRegions;
	size_t MemoryRegionCount = KeLoaderParameterBlock.MemoryRegionCount;
	
	for (uint64_t i = 0; i < MemoryRegionCount; i++)
	{
		// if the entry isn't usable, skip it
		PLOADER_MEMORY_REGION Entry = &MemoryRegions[i];
		
		if (Entry->Type != LOADER_MEM_FREE)
			continue;
		
		// Note! Usable entries in limine are guaranteed to be aligned to
		// page size, and not overlap any other entries. So we are good
		
		// if it's got no pages, also skip it..
		if (Entry->Size < Size)
			continue;
		
		uintptr_t CurrAddr = Entry->Base;
		Entry->Base += Size;
		Entry->Size -= Size;
		
		return CurrAddr;
	}
	
	KeCrashBeforeSMPInit("Error, out of memory in the memmap allocate function");
}

INIT
MMPFN MiAllocatePfnFromMemMap()
{
	uintptr_t Page = MiAllocatePageFromMemMap();
	if (!Page)
		return PFN_INVALID;
	
	return MmPhysPageToPFN(Page);
}

typedef struct
{
	uint64_t entries[512];
}
PageMapLevel;

#define PTE_ADDRESS(pte) MmPFNToPhysPage(MM_PTE_PFN(pte))

INIT
static bool MiMapNewPageAtAddressIfNeeded(uintptr_t pageTable, uintptr_t address)
{
#ifdef TARGET_AMD64
	// TODO: remove this arch-specific implementation.

	// Maps a new page at an address, if needed.
	PageMapLevel *pPML[4];
	pPML[3] = (PageMapLevel*) MmGetHHDMOffsetAddr(pageTable);
	
	for (int i = 3; i >= 0; i--)
	{
		int index = (address >> (12 + 9 * i)) & 0x1FF;
		if (MM_PTE_ISPRESENT(pPML[i]->entries[index]))
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
	(void) pageTable;

	if (!MmCheckPteLocationAllocator(address, true, MiAllocatePfnFromMemMap))
		return false;
	
	PMMPTE Pte = MmGetPteLocation(address);
	if (*Pte) {
		return true;
	}
	
	MMPFN Pfn = MiAllocatePfnFromMemMap();
	if (Pfn == PFN_INVALID) {
		return false;
	}
	
	MmBeginUsingHHDM();
	memset(MmGetHHDMOffsetAddr(MmPFNToPhysPage(Pfn)), 0, PAGE_SIZE);
	MmEndUsingHHDM();
	
	*Pte = MM_PTE_NEWPFN(Pfn) | MM_PTE_PRESENT | MM_PTE_READWRITE;
	
	return true;
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
#ifdef IS_64_BIT
	// with 4-level paging, limine seems to be hardcoded at this address, so we're probably good. Although
	// the protocol does NOT specify that, and it does seem to be affected by KASLR...
	if (KeLoaderParameterBlock.HhdmBase != 0xFFFF800000000000)
		DbgPrint("WARNING: The HHDM isn't at 0xFFFF 8000 0000 0000, things may go wrong! (It's actually at %p)", (void*) KeLoaderParameterBlock.HhdmBase);
#endif
	
#ifdef IS_64_BIT
	MmHHDMBase = KeLoaderParameterBlock.HhdmBase;
#endif
	
	uintptr_t currPageTablePhys = KeGetCurrentPageTable();
	
	// allocate the entries in the page frame number database
	PLOADER_MEMORY_REGION MemoryRegions = KeLoaderParameterBlock.MemoryRegions;
	size_t MemoryRegionCount = KeLoaderParameterBlock.MemoryRegionCount;
	
	// pass 0: print out all the entries for debugging
#ifdef PMMDEBUG
	for (uint64_t i = 0; i < MemoryRegionCount; i++)
	{
		PLOADER_MEMORY_REGION Entry = &MemoryRegions[i];
		
		if (Entry->Type != LOADER_MEM_FREE &&
			Entry->Type != LOADER_MEM_LOADER_RECLAIMABLE &&
			Entry->Type != LOADER_MEM_LOADED_PROGRAM)
			continue;
		
		DbgPrint("%p-%p (%d pages, %d)", Entry->Base, Entry->Base + Entry->Size, Entry->Size / PAGE_SIZE, Entry->Type);
	}
#endif
	
	// pass 1: mapping the pages themselves
	int numAllocatedPages = 0;
	for (uint64_t i = 0; i < MemoryRegionCount; i++)
	{
		PLOADER_MEMORY_REGION pEntry = &MemoryRegions[i];
		
		if (pEntry->Type != LOADER_MEM_FREE &&
			pEntry->Type != LOADER_MEM_LOADER_RECLAIMABLE &&
			pEntry->Type != LOADER_MEM_LOADED_PROGRAM)
			continue;
		
		// make a copy since we might tamper with this
		LOADER_MEMORY_REGION Entry = *pEntry;
		
		MMPFN pfnStart = MmPhysPageToPFN(Entry.Base);
		MMPFN pfnEnd   = MmPhysPageToPFN(Entry.Base + Entry.Size);
		
		uint64_t lastAllocatedPage = 0;
		for (MMPFN pfn = pfnStart; pfn < pfnEnd; pfn++)
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
	
	PmmDbgPrint("Initializing the PFN database.", sizeof(MMPFDBE));
	// pass 2: Initting the PFN database
	MMPFN lastPfnOfPrevBlock = PFN_INVALID;
	
	int TotalPageCount = 0, FreePageCount = 0, ReclaimPageCount = 0;
	
	for (uint64_t i = 0; i < MemoryRegionCount; i++)
	{
		PLOADER_MEMORY_REGION Entry = &MemoryRegions[i];
		
		if (Entry->Type != LOADER_MEM_FREE &&
			Entry->Type != LOADER_MEM_LOADER_RECLAIMABLE &&
			Entry->Type != LOADER_MEM_LOADED_PROGRAM)
			continue;
		
		for (uint64_t j = 0; j < Entry->Size; j += PAGE_SIZE)
		{
			bool isUsed = Entry->Type != LOADER_MEM_FREE;
			
			int currPFN = MmPhysPageToPFN(Entry->Base + j);
			
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
				
				if (j + PAGE_SIZE >= Entry->Size)
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

void MmRegisterMMIOAsMemory(uintptr_t Base, uintptr_t Length)
{
	Length += Base & (PAGE_SIZE - 1);
	Base   &= ~(PAGE_SIZE - 1);
	Length  = (Length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	
	MMPFN PfnStart = MmPhysPageToPFN(Base);
	MMPFN PfnEnd   = MmPhysPageToPFN(Base + Length);
	
	// NOTE: I don't think we need the PFDB locked here.
	//
	// Nobody should access this region of PFDB before we
	// finish initializing.
	
	uintptr_t lastAllocatedPage = 0;
	for (MMPFN Pfn = PfnStart; Pfn < PfnEnd; Pfn++)
	{
		uintptr_t currPage = (MM_PFNDB_BASE + sizeof(MMPFDBE) * Pfn) & ~(PAGE_SIZE - 1);
		if (lastAllocatedPage != currPage)
		{
			if (!MmCheckPteLocation(currPage, true))
				KeCrash("MmRegisterMMIOAsMemory: could not ensure PTE location %p exists", currPage);
			
			PMMPTE Pte = MmGetPteLocation(currPage);
			if (!MM_PTE_ISPRESENT(*Pte))
			{
				// allocate it
				MMPFN PfnAlloc = MmAllocatePhysicalPage();
				
				*Pte = ((uintptr_t)PfnAlloc * PAGE_SIZE) | MM_PTE_PRESENT | MM_PTE_READWRITE | MM_PTE_NOEXEC | MM_PTE_ISFROMPMM;
				
				KeInvalidatePage((void*) currPage);
			}
			
			lastAllocatedPage = currPage;
		}
		
		PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
		memset(Pfdbe, 0, sizeof *Pfdbe);
		
		Pfdbe->Type = PF_TYPE_MMIO;
		Pfdbe->RefCount = 1;
	}
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
	ASSERT(Pfn != PFN_INVALID);
	ASSERT(MmPfnLock.Locked);
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	
	ASSERT(Pfdbe->FileCache._PrototypePte && "How did we recover this page?!");
	
	MmpUnlinkPfn(Pfdbe);
	MmpEnsurePfnIsntEndsOfList(&MiFirstStandbyPFN, &MiLastStandbyPFN, Pfdbe, Pfn);
	MmpEnsurePfnIsntEndsOfList(&MiFirstModifiedPFN, &MiLastModifiedPFN, Pfdbe, Pfn);
	
	// Now that the PFN is unlinked, we can turn it into a used PFN.
	Pfdbe->RefCount = 1;
	Pfdbe->NextFrame = 0;
	Pfdbe->PrevFrame = 0;
	Pfdbe->Type = PF_TYPE_USED;
}

static void MmpAddPfnToList(PMMPFN First, PMMPFN Last, MMPFN Current)
{
	ASSERT(Current != PFN_INVALID);
	
	if (*First == PFN_INVALID)
	{
		*First = *Last = Current;
		
		MMPFDBE* pPF = MmGetPageFrameFromPFN(Current);
		pPF->NextFrame = PFN_INVALID;
		pPF->PrevFrame = PFN_INVALID;
		return;
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
		//
		// ^^^^^ this is old as hell and probably outdated TODO: clarify or remove
		ASSERT(Pfdbe->FileCache._PrototypePte);
		
		MM_PROTOTYPE_PTE_PTR Ptr = PFDBE_PrototypePte(Pfdbe);
		
#ifdef IS_64_BIT

		AtStore(*Ptr, MM_SLA_NO_DATA);

#else
		if (Ptr & MM_PROTO_PTE_PTR_IS_VIRTUAL)
		{
			// Virtual
			AtStore(*(uintptr_t*)(Ptr & ~MM_PROTO_PTE_PTR_IS_VIRTUAL), MM_SLA_NO_DATA);
		}
		else
		{
			// Physical
			MmBeginUsingHHDM();
			AtStore(*(uintptr_t*)MmGetHHDMOffsetAddr(Ptr), MM_SLA_NO_DATA);
			MmEndUsingHHDM();
		}
#endif
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
	
	MMPFN currPFN = *First;
	PMMPFDBE pPF = MmGetPageFrameFromPFN(*First);
	
	MmpRemovePfnFromList(First, Last, *First);
	MmpInitializePfn(pPF);
	
	ASSERT(currPFN != PFN_INVALID && currPFN != MM_PFN_OUTOFMEMORY);
	return currPFN;
}

MMPFN MiAllocatePhysicalPageWithPfdbLocked(bool* IsZeroed)
{
	*IsZeroed = true;
	MMPFN currPFN = MmpAllocateFromFreeList(&MiFirstZeroPFN, &MiLastZeroPFN);
	if (currPFN == PFN_INVALID)
	{
		*IsZeroed = false;
		currPFN = MmpAllocateFromFreeList(&MiFirstFreePFN, &MiLastFreePFN);
		if (currPFN == PFN_INVALID)
			currPFN = MmpAllocateFromFreeList(&MiFirstStandbyPFN, &MiLastStandbyPFN);
	}
	
	return currPFN;
}

MMPFN MmAllocatePhysicalPage()
{
	bool FromZero;
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	MMPFN currPFN = MiAllocatePhysicalPageWithPfdbLocked(&FromZero);
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
	
	if (!FromZero && currPFN != PFN_INVALID)
	{
		MmBeginUsingHHDM();
		memset(MmGetHHDMOffsetAddr(MmPFNToPhysPage(currPFN)), 0, PAGE_SIZE);
		MmEndUsingHHDM();
	}
	
#ifdef PMMDEBUG
	DbgPrint("MmAllocatePhysicalPage() => %d (RA:%p)", currPFN, __builtin_return_address(0));
#endif
	
	return currPFN;
}

MMPFN MiRemoveOneModifiedPfn()
{
	ASSERT(MmPfnLock.Locked);
	
	MMPFN Pfn = MmpAllocateFromFreeList(&MiFirstModifiedPFN, &MiLastModifiedPFN);
	if (Pfn == PFN_INVALID)
		return Pfn;
	
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	Pfdbe->IsInModifiedPageList = false;
	
#ifdef PMMDEBUG
	DbgPrint("MiRemoveOneModifiedPfn() => %d (RA:%p)", Pfn, __builtin_return_address(0));
#endif
	
	return Pfn;
}

void MmSetPrototypePtePfn(MMPFN Pfn, MM_PROTOTYPE_PTE_PTR PrototypePte)
{
	ASSERT(Pfn != PFN_INVALID);
	
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	// If this page is freed after this operation, then the prototype
	// PTE will be atomically set to zero when reclaimed.
	MmGetPageFrameFromPFN(Pfn)->FileCache._PrototypePte = (uintptr_t) PrototypePte;
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

void MmSetCacheDetailsPfn(MMPFN Pfn, PFCB Fcb, uint64_t Offset)
{
	ASSERT(Pfn != PFN_INVALID);
	
	Offset /= PAGE_SIZE;
	
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	
	Pfdbe->FileCache._Fcb = (uintptr_t) Fcb;
	Pfdbe->FileCache._OffsetLower = Offset;
	Pfdbe->_OffsetUpper = (Offset >> 32);
	Pfdbe->IsFileCache = 1;
	Pfdbe->Modified = 0;
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

void MmSetModifiedPfn(MMPFN Pfn)
{
	ASSERT(Pfn != PFN_INVALID);
	
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	
	// This function must only be called for allocated pages.
	ASSERT(Pfdbe->Type == PF_TYPE_USED);
	
	Pfdbe->Modified = 1;
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

extern KEVENT MmModifiedPageWriterEvent;

void MmFreePhysicalPage(MMPFN pfn)
{
	ASSERT(pfn != PFN_INVALID);
	
	KIPL OldIpl;
	
#ifdef PMMDEBUG
	DbgPrint("MmFreePhysicalPage()     <= %d (RA:%p)", pfn, __builtin_return_address(0));
#endif
	
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	PMMPFDBE PageFrame = MmGetPageFrameFromPFN(pfn);
	
	ASSERT(PageFrame->Type == PF_TYPE_USED || PageFrame->Type == PF_TYPE_RECLAIM || PageFrame->Type == PF_TYPE_MMIO);
	
	int RefCount = --PageFrame->RefCount;
	ASSERT(RefCount >= 0);
	
	if (RefCount <= 0)
	{
#ifdef DEBUG
		if (PageFrame->Type == PF_TYPE_RECLAIM)
			MmReclaimedPageCount++;
		
		if (PageFrame->Type == PF_TYPE_MMIO)
			KeCrash("MmFreePhysicalPage: attempted to free an MMIO page (PFN %d)", pfn);
#endif
		
		PageFrame->IsInModifiedPageList = false;
		if (PageFrame->FileCache._PrototypePte)
		{
			// This is part of a cache control block.  Was this written?
			if (PageFrame->Modified)
			{
				// Yes, we should add it to the modified list. However, this
				// page will remain marked as "used" until the modified page
				// writer actually writes to the page.
				//
				// TODO: Signal the modified page writter to start writing,
				// when pressure builds up on the modified page list or when
				// memory is running low.
				MmpAddPfnToList(&MiFirstModifiedPFN, &MiLastModifiedPFN, pfn);
				PageFrame->IsInModifiedPageList = true;
				
				// TEMPORARY TEMPORARY
				// TODO: Only signal the modified page writer event when significant
				// build up has happened, or shutdown is requested, or something else.
				// This is just for debugging.
				KeSetEvent(&MmModifiedPageWriterEvent, 100);
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

void MiTransformPageToStandbyPfn(MMPFN Pfn)
{
	ASSERT(Pfn != PFN_INVALID);
	ASSERT(MmPfnLock.Locked);
	
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	
	// If this page has transitioned back into actual use, then its reference count
	// will be higher than zero, so we will be able to ignore it.
	if (Pfdbe->RefCount > 0 || Pfdbe->Type != PF_TYPE_USED)
	{
		DbgPrint(
			"MiTransformPageToStandbyPfn: Not transforming pfn %d to standby as its type is "
			"%d and refcount is %d.",
			Pfn,
			Pfdbe->Type,
			Pfdbe->RefCount
		);
		
		return;
	}
	
	// If this page is in the modified list, then it has been modified after it was
	// removed by the modified page writer worker.  We need to flush these changes too.
	if (Pfdbe->IsInModifiedPageList)
	{
		DbgPrint(
			"MiTransformPageToStandbyPfn: Not transforming pfn %d to standby as it was "
			"re-added into the modified list.",
			Pfn
		);
		
		return;
	}
	
	MmpAddPfnToList(&MiFirstStandbyPFN, &MiLastStandbyPFN, Pfn);
	Pfdbe->Type = PF_TYPE_TRANSITION;
	Pfdbe->Modified = 0;
	MmTotalFreePages++;
}

void MiReinsertIntoModifiedList(MMPFN Pfn)
{
	ASSERT(Pfn != PFN_INVALID);
	ASSERT(MmPfnLock.Locked);
	
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	
	if (Pfdbe->RefCount > 0 || Pfdbe->Type != PF_TYPE_USED)
		return;
	
	if (!Pfdbe->IsInModifiedPageList)
	{
		MmpAddPfnToList(&MiFirstModifiedPFN, &MiLastModifiedPFN, Pfn);
		Pfdbe->IsInModifiedPageList = true;
		Pfdbe->Modified = true;
	}
}

// Zeroes out the first free PFN, takes it off the free PFN list and
// adds it to the zero PFN list.
void MmZeroOutFirstPFN()
{
	// step 1. find the first free PFN, if it exists.
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	if (MiFirstFreePFN == PFN_INVALID)
	{
	Return:
		KeReleaseSpinLock(&MmPfnLock, OldIpl);
		return;
	}

	MMPFN pfn = MiFirstFreePFN;
	PMMPFDBE pPF = MmGetPageFrameFromPFN(pfn);
	
	if (pPF->Type == PF_TYPE_ZEROED)
		goto Return;

#ifdef DEBUG	
	if (pPF->Type != PF_TYPE_FREE)
		KeCrash("Error, attempting to zero out pfn %d which is used", pfn);
#endif
	
	pPF->Type = PF_TYPE_ZEROED;
	
	MmpRemovePfnFromList(&MiFirstFreePFN, &MiLastFreePFN, pfn);
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
	
	// step 2. free the PFN.
	MmBeginUsingHHDM();
	uint8_t* mem = MmGetHHDMOffsetAddr(MmPFNToPhysPage(pfn));
	memset(mem, 0, PAGE_SIZE);
	MmEndUsingHHDM();
	
	// step 3. add this PFN to the zero list
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	MmpAddPfnToList(&MiFirstZeroPFN, &MiLastZeroPFN, pfn);
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

#ifdef IS_64_BIT

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

#else

void* MmAllocatePhysicalPageHHDM()
{
	KeCrash("NYI MmAllocatePhysicalPageHHDM");
}

void MmFreePhysicalPageHHDM(void* Page)
{
	KeCrash("NYI MmFreePhysicalPageHHDM(%p)", Page);
}

#endif

void MiPageAddReferenceWithPfdbLocked(MMPFN Pfn)
{
	ASSERT(Pfn != PFN_INVALID);
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
	ASSERT(Pfn != PFN_INVALID);
	ASSERT(MmPfnLock.Locked);
	
	PMMPFDBE PageFrame = MmGetPageFrameFromPFN(Pfn);
	PageFrame->Modified = true;
}

void MmSetModifiedPage(MMPFN Pfn)
{
	ASSERT(Pfn != PFN_INVALID);
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	MiSetModifiedPageWithPfdbLocked(Pfn);
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

void MmPageAddReference(MMPFN Pfn)
{
#ifdef PMMDEBUG
	DbgPrint("MmPageAddReference    () => %d (RA:%p)", Pfn, __builtin_return_address(0));
#endif
	
	ASSERT(Pfn != PFN_INVALID);
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	MiPageAddReferenceWithPfdbLocked(Pfn);
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

int MiGetReferenceCountPfn(MMPFN Pfn)
{
	ASSERT(Pfn != PFN_INVALID);
	ASSERT(MmPfnLock.Locked);
	
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	if (Pfdbe->Type != PF_TYPE_USED)
		return 0;
	
	return Pfdbe->RefCount;
}

FORCE_INLINE
bool MmpIsFree(MMPFN Pfn)
{
	return MmGetPageFrameFromPFN(Pfn)->Type == PF_TYPE_FREE;
}

static void MmpRemovePfnFromItsList(MMPFN Pfn)
{
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	
	ASSERT(Pfdbe->Type == PF_TYPE_FREE);
	
	if (Pfdbe->NextFrame != PFN_INVALID && Pfdbe->PrevFrame != PFN_INVALID) {
		MmpUnlinkPfn(Pfdbe);
		return;
	}
	
	PMMPFN First = NULL, Last = NULL;
	if (Pfdbe->NextFrame == PFN_INVALID) {
		if (MiLastFreePFN == Pfn)
			First = &MiFirstFreePFN, Last = &MiLastFreePFN;
		else if (MiLastZeroPFN == Pfn)
			First = &MiFirstZeroPFN, Last = &MiLastZeroPFN;
		else if (MiLastStandbyPFN == Pfn)
			First = &MiFirstStandbyPFN, Last = &MiLastStandbyPFN;
	}
	else if (Pfdbe->PrevFrame == PFN_INVALID) {
		if (MiFirstFreePFN == Pfn)
			First = &MiFirstFreePFN, Last = &MiLastFreePFN;
		else if (MiFirstZeroPFN == Pfn)
			First = &MiFirstZeroPFN, Last = &MiLastZeroPFN;
		else if (MiFirstStandbyPFN == Pfn)
			First = &MiFirstStandbyPFN, Last = &MiLastStandbyPFN;
	}
	else {
		ASSERT(!"Neither NextFrame nor PrevFrame are NULL but this is impossible");
	}
	
	ASSERT(First && Last && "This PFN is in a free list... right?!");
	MmpRemovePfnFromList(First, Last, Pfn);
}

static bool MmpTryAllocateContiguousRegion(MMPFN Pfn, int PageCount, uintptr_t Alignment)
{
	// first, make sure this region is truly 16 Kbyte aligned.
	if (MmPFNToPhysPage(Pfn) & Alignment)
		return false;
	
	// N.B. Pfn is already free
	ASSERT(MmpIsFree(Pfn));
	
	for (int i = 1; i < PageCount; i++) {
		if (!MmpIsFree(Pfn + i))
			return false;
	}
	
	// okay, now actually allocate it.
	for (int i = 0; i < PageCount; i++) {
		MmpRemovePfnFromItsList(Pfn + i);
		MmpInitializePfn(MmGetPageFrameFromPFN(Pfn + i));
	}
	
	return true;
}

MMPFN MmAllocatePhysicalContiguousRegion(int PageCount, uintptr_t Alignment)
{
	MMPFN Pfn;
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	for (Pfn = MiFirstFreePFN; Pfn != MiLastFreePFN; Pfn = MmGetPageFrameFromPFN(Pfn)->NextFrame) {
		if (MmpTryAllocateContiguousRegion(Pfn, PageCount, Alignment)) {
			goto Return;
		}
	}
	
	for (Pfn = MiFirstZeroPFN; Pfn != MiLastZeroPFN; Pfn = MmGetPageFrameFromPFN(Pfn)->NextFrame) {
		if (MmpTryAllocateContiguousRegion(Pfn, PageCount, Alignment)) {
			goto Return;
		}
	}
	
	for (Pfn = MiFirstStandbyPFN; Pfn != MiLastStandbyPFN; Pfn = MmGetPageFrameFromPFN(Pfn)->NextFrame) {
		if (MmpTryAllocateContiguousRegion(Pfn, PageCount, Alignment)) {
			goto Return;
		}
	}
	
Return:
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
	return Pfn;
}

void MmFreePhysicalContiguousRegion(MMPFN PfnStart, int PageCount)
{
	for (int i = 0; i < PageCount; i++)
		MmFreePhysicalPage(PfnStart + i);
}
