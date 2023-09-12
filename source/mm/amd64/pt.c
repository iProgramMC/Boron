// Boron64 - Amd64 page table management
#include <main.h>
#include <string.h>
#include <mm.h>
#include <arch/amd64.h>

// Creates a page mapping.
PageMapping MmCreatePageMapping(PageMapping OldPageMapping)
{
	// Allocate the PML4.
	int NewPageMappingPFN = MmAllocatePhysicalPage();
	if (NewPageMappingPFN == PFN_INVALID)
	{
		LogMsg("Error, can't create a new page mapping.  Can't allocate PML4");
		return 0;
	}
	
	uintptr_t NewPageMappingResult = MmPFNToPhysPage (NewPageMappingPFN);
	uint64_t* NewPageMappingAccess = MmGetHHDMOffsetAddr (NewPageMappingResult);
	uint64_t* OldPageMappingAccess = MmGetHHDMOffsetAddr (OldPageMapping);
	
	// copy the kernel's 256 entries, and zero out the first 256
	for (int i = 0; i < 256; i++)
	{
		NewPageMappingAccess[i] = 0;
	}
	
	for (int i = 256; i < 512; i++)
	{
		NewPageMappingAccess[i] = OldPageMappingAccess[i];
	}
	
	return (PageMapping) NewPageMappingResult;
}

bool MmpCloneUserHalfLevel(int Level, uint64_t* New, uint64_t* Old, int Index)
{
	New[Index] = 0;
	
	int PageForThisLevelPFN = MmAllocatePhysicalPage();
	if (PageForThisLevelPFN == PFN_INVALID)
		return false;
	
	if (~Old[Index] & MM_PTE_PRESENT)
	{
		// TODO handle a non present page... Currently just exit.
		New[Index] = 0;
		return true;
	}
	
	if (Level == 1)
	{
		// TODO: Copy on write
		// return true;
	}
	
	uintptr_t PageForThisLevel = MmPFNToPhysPage (PageForThisLevelPFN);
	
	// write it to the new with the same flags as old, except that we set the PMM flag
	New[Index] = PageForThisLevel | (Old[Index] & 0xFFF) | MM_PTE_ISFROMPMM;
	
	uint64_t* PageAccess = MmGetHHDMOffsetAddr (PageForThisLevel);
	
	if (Level == 1)
	{
		// Copy the contents of the page.
		uint64_t* OldPageAccess = MmGetHHDMOffsetAddr (Old[Index] & MM_PTE_ADDRESSMASK);
		
		memcpy (PageAccess, OldPageAccess, PAGE_SIZE);
		
		return true;
	}
	
	// Recursively copy the next level.
	for (int i = 0; i < 512; i++)
	{
		uint64_t* OldPageAccess = MmGetHHDMOffsetAddr (Old[Index] & MM_PTE_ADDRESSMASK);
		
		MmpCloneUserHalfLevel (Level - 1, PageAccess, OldPageAccess, i);
	}
	
	return true;
}

// Clones a user page mapping
PageMapping MmClonePageMapping(PageMapping OldPageMapping)
{
	PageMapping NewPageMapping = MmCreatePageMapping(OldPageMapping);
	
	// If the new page mapping is zero, we can't proceed!
	if (NewPageMapping == 0)
		return 0;
	
	uint64_t* NewPageMappingAccess = MmGetHHDMOffsetAddr (NewPageMapping);
	uint64_t* OldPageMappingAccess = MmGetHHDMOffsetAddr (OldPageMapping);
	
	// Clone the user half now.
	for (int i = 0; i < 256; i++)
	{
		if (!MmpCloneUserHalfLevel(4, NewPageMappingAccess, OldPageMappingAccess, i))
		{
			LogMsg("Error, can't fully clone a page mapping. i = %d", i);
			
			// TODO: rollback all clones so far
			
			MmFreePhysicalPageHHDM(NewPageMappingAccess);
			return 0;
		}
	}
	
	return NewPageMapping;
}
