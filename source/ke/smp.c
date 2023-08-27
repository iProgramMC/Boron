// Boron64 - SMP initialization
#include <ke.h>
#include <mm.h>
#include <hal.h>
#include <string.h>
#include <_limine.h>

volatile struct limine_smp_request KeLimineSmpRequest =
{
	.id = LIMINE_SMP_REQUEST,
	.revision = 0,
	.response = NULL,
	.flags = 0,
};

void KeHandleDoubleFault(CPUState* State)
{
	uintptr_t FaultPC, FaultCode;
#ifdef TARGET_AMD64
	FaultPC     = State->rip;
	FaultCode   = State->error_code;
#else
#	error "Add a double fault handler for your own platform!"
#endif
	
	LogMsg("Double fault at %p (error code %d)", FaultPC, FaultCode);
	
	// TODO: crash properly
	KeStopCurrentCPU();
}

void KeHandlePageFault(CPUState* State)
{
	uintptr_t FaultPC, FaultTarget, FaultCode;
	
#ifdef TARGET_AMD64
	FaultPC     = State->rip;
	FaultTarget = State->cr2;
	FaultCode   = State->error_code;
#else
#	error "Add a page fault handler for your own platform!"
#endif
	
	LogMsg("Page fault at %p (tried to access %p, error code %d) on CPU %u", FaultPC, FaultTarget, FaultCode, KeGetCPU()->m_apicID);
	
	// TODO: crash properly
	KeStopCurrentCPU();
}

// An atomic write to this field causes the parked CPU to jump to the written address,
// on a 64KiB (or Stack Size Request size) stack. A pointer to the struct limine_smp_info
// structure of the CPU is passed in RDI
NO_RETURN void KiCPUBootstrap(struct limine_smp_info* pInfo)
{
	CPU *pCpu = (CPU *)pInfo->extra_argument;
	HalSetCPUPointer(pCpu);
	
	// Update the IPL when initing. Currently we start at the highest IPL
	HalOnUpdateIPL(0, KeGetIPL());
	
	HalInitCPU();
	// assign our own arch independent ISRs
	HalAssignISR(INT_DOUBLE_FAULT, KeHandleDoubleFault);
	HalAssignISR(INT_PAGE_FAULT,   KeHandlePageFault);
	
	LogMsg("Hello from CPU %u", (unsigned) pInfo->lapic_id);
	
	// issue a page fault right now
	*((uint64_t*)0x5adfdeadbeef) = 420;
	
	KeStopCurrentCPU();
}

CPU* KeGetCPU()
{
	return HalGetCPUPointer();
}

eIPL KeGetIPL()
{
	CPU* thisCPU = KeGetCPU();
	return thisCPU->m_ipl;
}

eIPL KeIPLRaise(eIPL newIPL)
{
	CPU* thisCPU = KeGetCPU();
	eIPL oldIPL = thisCPU->m_ipl;
	
	if (oldIPL == newIPL)
		return oldIPL; // no changes
	
	if (oldIPL > newIPL)
	{
		LogMsg("Error, can't raise the IPL to a lower level than we are (old %d, new %d)", oldIPL, newIPL);
		// TODO: panic here
		return oldIPL;
	}
	
	HalOnUpdateIPL(newIPL, oldIPL);
	
	// Set the current IPL
	thisCPU->m_ipl = newIPL;
	return oldIPL;
}

// similar logic, except we will also call DPCs if needed
eIPL KeIPLLower(eIPL newIPL)
{
	CPU* thisCPU = KeGetCPU();
	eIPL oldIPL = thisCPU->m_ipl;
	
	if (oldIPL == newIPL)
		return oldIPL; // no changes
	
	if (oldIPL > newIPL)
	{
		LogMsg("Error, can't raise the IPL to a lower level than we are (old %d, new %d)", oldIPL, newIPL);
		// TODO: panic here
		return oldIPL;
	}
	
	// Set the current IPL
	thisCPU->m_ipl = newIPL;
	
	HalOnUpdateIPL(newIPL, oldIPL);
	
	// TODO (updated): Issue a self-IPI to call DPCs.
	
	// TODO: call DPCs
	// ideally we'd have something as follows:
	// while (thisCPU->m_ipl > newIPL) {
	//     thisCPU->m_ipl--;  // lower the IPL by one stage..
	//     executeDPCs();     // execute the DPCs at this IPL
	// }                      // continue until we are at the current IPL
	// executeDPCs();         // do that one more time because we weren't in the while loop
	
	return oldIPL;
}

NO_RETURN void KeInitSMP()
{
	struct limine_smp_response* pSMP = KeLimineSmpRequest.response;
	struct limine_smp_info* pBSPInfo = NULL;
	
	// Initialize all the CPUs in series.
	for (uint64_t i = 0; i < pSMP->cpu_count; i++)
	{
		bool bIsBSP = pSMP->bsp_lapic_id == pSMP->cpus[i]->lapic_id;
		
		// Allocate space for the CPU struct.
		int cpuPFN = MmAllocatePhysicalPage();
		if (cpuPFN == PFN_INVALID)
		{
			LogMsg("Error, can't initialize CPUs, we don't have enough memory");
			KeStopCurrentCPU();
		}
		
		CPU* pCPU = MmGetHHDMOffsetAddr(MmPFNToPhysPage(cpuPFN));
		memset(pCPU, 0, sizeof * pCPU);
		
		struct limine_smp_info* pInfo = pSMP->cpus[i];
		
		// initialize the struct
		pCPU->m_apicID    = pInfo->lapic_id;
		pCPU->m_bIsBSP    = bIsBSP;
		pCPU->m_pSMPInfo  = pInfo;
		pCPU->m_pageTable = HalGetCurrentPageTable(); // it'd better be that
		pCPU->m_ipl       = IPL_NOINTS;  // run it at the highest IPL for now. We'll lower it later
		
		pInfo->extra_argument = (uint64_t)pCPU;
		
		// Launch the CPU! Note, we won't actually go there if we are the bootstrap processor.
		AtStore(pInfo->goto_address, &KiCPUBootstrap);
		
		if (bIsBSP)
			pBSPInfo = pInfo;
	}
	
	KiCPUBootstrap(pBSPInfo);
}
