#include <mm.h>
#include <hal.h>

// Returns: Whether the page fault was fixed or not
bool MmPageFault(UNUSED uintptr_t FaultPC, UNUSED uintptr_t FaultAddress, UNUSED uintptr_t FaultMode)
{
	// TODO
	return false;
}

// forces all cores to issue a TLB shootdown (invalidate the address from the
// TLB - with invlpg on amd64 for instance)
void MmIssueTLBShootDown(uintptr_t Address, size_t Length)
{
	HalIssueTLBShootDown(Address, Length);
}
