// Boron64 - Memory mapping functions.
// - These allow mutation and cloning of the current page table.
#include <mm.h>
#include <hal.h>

void MmClonePageTableLevel(uintptr_t new_pl, uintptr_t old_pl, uintptr_t base_address, int level, bool copy_user_half, bool invalidate_if_updating_old_pt)
{
	PageMapLevel* pNewPL = (PageMapLevel*) MmGetHHDMOffsetAddr(new_pl);
	PageMapLevel* pOldPL = (PageMapLevel*) MmGetHHDMOffsetAddr(old_pl);
	
	// if we are messing with the PML1
	if (level == 1)
	{
		// 
		
		return;
	}
	
	const uintptr_t multiplier = 1 << (12 + 9 * (level - 1));
	
	for (int i = 0; i < 512; i++)
	{
		if (i >= 256 || copy_user_half)
		{
			uint64_t old_entry = pOldPL->entries[i];
			
			if (~old_entry & MM_PTE_PRESENT) // not present
			{
				pNewPL->entries[i] = pOldPL->entries[i];
			}
			else
			{
				int new_pfn = MmAllocatePhysicalPage();
				if (new_pfn == PFN_INVALID)
				{
					LogMsg("ERROR, out of memory, cannot clone the page table.");
					// TODO, recover??
					return;
				}
			}
		}
		else
		{
			pNewPL->entries[i] = 0;
		}
	}
}

uintptr_t MmClonePageTable(uintptr_t pt, bool copy_user_half)
{
	// allocate a PML4
	int pfn_pml4 = MmAllocatePhysicalPage();
	if (pfn_pml4 == PFN_INVALID)
	{
		LogMsg("Error, out of memory, can't clone a page table");
		// TODO, recover??
		return 0;
	}
	
	uintptr_t new_pt = MmPFNToPhysPage(pfn_pml4);
	
	MmClonePageTableLevel (new_pt, pt, 0x0, 4, copy_user_half, pt == KeGetCurrentPageTable());
	
	return new_pt;
}
