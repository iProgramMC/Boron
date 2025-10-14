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

//extern volatile struct limine_hhdm_request   KeLimineHhdmRequest;
//extern volatile struct limine_memmap_request KeLimineMemMapRequest;

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

static void MiUpdateHHDMWindowBase(uintptr_t PhysAddr)
{
	PMMPTE Ptes = (PMMPTE)(MI_PML1_LOCATION);
	
	PhysAddr &= MI_FASTMAP_MASK;
	MmHHDMWindowBase = PhysAddr;
	
	for (size_t i = 0; i < 8 * 1024 * 1024; i += 4096)
	{
		uintptr_t Address = MI_FASTMAP_START + i;
		
		MMADDRESS_CONVERT Convert;
		Convert.Long = Address;
		
		Ptes[Convert.Level2Index * 1024 + Convert.Level1Index] = MM_PTE_PRESENT | MM_PTE_READWRITE | MM_PTE_NOEXEC | (PhysAddr + i);
		KeInvalidatePage((void*)Address);
	}
}

void* MmGetHHDMOffsetAddr(uintptr_t PhysAddr)
{
	if ((PhysAddr & MI_FASTMAP_MASK) != MmHHDMWindowBase)
		MiUpdateHHDMWindowBase(PhysAddr);
	
	return (void*)(MI_FASTMAP_START + (PhysAddr & ~MI_FASTMAP_MASK));
}

uintptr_t MmGetHHDMOffsetFromAddr(void* Addr)
{
	uintptr_t AddrInt = (uintptr_t) Addr;
	
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
		
		// Note! Usable entries in limine are guaranteed to be aligned to
		// page size, and not overlap any other entries. So we are good
		
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
#elif defined TARGET_I386
	(void)pageTable; // unused
	
	MMADDRESS_CONVERT Convert;
	Convert.Long = address;
	
	PMMPTE Level1, Level2;
	
	Level2 = (PMMPTE)MI_PML2_LOCATION;
	Level1 = (PMMPTE)(MI_PML1_LOCATION + 4096 * Convert.Level2Index);
	
	if (~Level2[Convert.Level2Index] & MM_PTE_PRESENT)
	{
		uintptr_t Addr = MiAllocatePageFromMemMap();
		
		if (!Addr)
		{
			// TODO: Allow rollback
			return false;
		}
		
		Level2[Convert.Level2Index] = Addr | MM_PTE_PRESENT | MM_PTE_READWRITE;
	}
	
	if (~Level1[Convert.Level1Index] & MM_PTE_PRESENT)
	{
		uintptr_t Addr = MiAllocatePageFromMemMap();
		
		if (!Addr)
		{
			// TODO: Allow rollback
			return false;
		}
		
		memset(MmGetHHDMOffsetAddr(Addr), 0, PAGE_SIZE);
		Level1[Convert.Level1Index] = Addr | MM_PTE_PRESENT | MM_PTE_READWRITE;
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
	int lastPfnOfPrevBlock = PFN_INVALID;
	
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
			bool isUsed = Entry->Type != LIMINE_MEMMAP_USABLE;
			
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
			if (!(*Pte & MM_PTE_PRESENT))
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
	
	bool FromZero = true;
	MMPFN currPFN = MmpAllocateFromFreeList(&MiFirstZeroPFN, &MiLastZeroPFN);
	if (currPFN == PFN_INVALID)
	{
		FromZero = false;
		currPFN = MmpAllocateFromFreeList(&MiFirstFreePFN, &MiLastFreePFN);
		if (currPFN == PFN_INVALID)
			currPFN = MmpAllocateFromFreeList(&MiFirstStandbyPFN, &MiLastStandbyPFN);
	}
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
	
	if (!FromZero && currPFN != PFN_INVALID)
		memset(MmGetHHDMOffsetAddr(MmPFNToPhysPage(currPFN)), 0, PAGE_SIZE);
	
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

void MmSetPrototypePtePfn(MMPFN Pfn, uintptr_t* PrototypePte)
{
	ASSERT(Pfn != PFN_INVALID);
	
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	// If this page is freed after this operation, then the prototype
	// PTE will be atomically set to zero when reclaimed.
	MmGetPageFrameFromPFN(Pfn)->FileCache._PrototypePte = (uint64_t) PrototypePte;
	
	KeReleaseSpinLock(&MmPfnLock, OldIpl);
}

void MmSetCacheDetailsPfn(MMPFN Pfn, uintptr_t* PrototypePte, PFCB Fcb, uint64_t Offset)
{
	ASSERT(Pfn != PFN_INVALID);
	
	Offset /= PAGE_SIZE;
	
	KIPL OldIpl;
	KeAcquireSpinLock(&MmPfnLock, &OldIpl);
	
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	
	Pfdbe->FileCache._PrototypePte = (uint64_t) PrototypePte;
	Pfdbe->FileCache._Fcb = (uint64_t) Fcb;
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

// Zeroes out a free PFN, takes it off the free PFN list and adds it to
// the zero PFN list.
static void MmpZeroOutPFN(MMPFN pfn)
{
	ASSERT(pfn != PFN_INVALID);
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
	ASSERT(pfn != PFN_INVALID);
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
