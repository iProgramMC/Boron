// Boron64 - SMP initialization
#include <ke.h>
#include <mm.h>
#include <arch.h>
#include <string.h>
#include <_limine.h>

volatile struct limine_smp_request KeLimineSmpRequest =
{
	.id = LIMINE_SMP_REQUEST,
	.revision = 0,
	.response = NULL,
	.flags = 0,
};

// TODO: Allow grabbing the interrupt code. For simplicity that isn't done
void KeOnUnknownInterrupt(uintptr_t FaultPC)
{
	LogMsg("Unknown interrupt at %p on CPU %u", FaultPC, KeGetCPU()->LapicId);
	
	// TODO: crash properly
	KeStopCurrentCPU();
}

void KeOnDoubleFault(uintptr_t FaultPC)
{
	LogMsg("Double fault at %p on CPU %u", FaultPC, KeGetCPU()->LapicId);
	
	// TODO: crash properly
	KeStopCurrentCPU();
}

void KeOnProtectionFault(uintptr_t FaultPC)
{
	LogMsg("General Protection Fault at %p on CPU %u", FaultPC, KeGetCPU()->LapicId);
	
	// TODO: crash properly
	KeStopCurrentCPU();
}

void KeOnPageFault(uintptr_t FaultPC, uintptr_t FaultAddress, uintptr_t FaultMode)
{
	LogMsg("Page fault at %p (tried to access %p, error code %llx) on CPU %u", FaultPC, FaultAddress, FaultMode, KeGetCPU()->LapicId);
	
	// TODO: crash properly
	KeStopCurrentCPU();
}

// An atomic write to this field causes the parked CPU to jump to the written address,
// on a 64KiB (or Stack Size Request size) stack. A pointer to the struct limine_smp_info
// structure of the CPU is passed in RDI
NO_RETURN void KiCPUBootstrap(struct limine_smp_info* pInfo)
{
	CPU *pCpu = (CPU *)pInfo->extra_argument;
	KeSetCPUPointer(pCpu);
	
	// Update the IPL when initing. Currently we start at the highest IPL
	KeOnUpdateIPL(0, KeGetIPL());
	
	KeInitCPU();
	
	LogMsg("Hello from CPU %u", (unsigned) pInfo->lapic_id);
	
	// issue a page fault right now
	*((uint64_t*)0x5adfdeadbeef) = 420;
	
	KeStopCurrentCPU();
}

CPU* KeGetCPU()
{
	return KeGetCPUPointer();
}

eIPL KeGetIPL()
{
	CPU* thisCPU = KeGetCPU();
	return thisCPU->Ipl;
}

eIPL KeIPLRaise(eIPL newIPL)
{
	CPU* thisCPU = KeGetCPU();
	eIPL oldIPL = thisCPU->Ipl;
	
	if (oldIPL == newIPL)
		return oldIPL; // no changes
	
	if (oldIPL > newIPL)
	{
		LogMsg("Error, can't raise the IPL to a lower level than we are (old %d, new %d)", oldIPL, newIPL);
		// TODO: panic here
		return oldIPL;
	}
	
	KeOnUpdateIPL(newIPL, oldIPL);
	
	// Set the current IPL
	thisCPU->Ipl = newIPL;
	return oldIPL;
}

// similar logic, except we will also call DPCs if needed
eIPL KeIPLLower(eIPL newIPL)
{
	CPU* thisCPU = KeGetCPU();
	eIPL oldIPL = thisCPU->Ipl;
	
	if (oldIPL == newIPL)
		return oldIPL; // no changes
	
	if (oldIPL > newIPL)
	{
		LogMsg("Error, can't raise the IPL to a lower level than we are (old %d, new %d)", oldIPL, newIPL);
		// TODO: panic here
		return oldIPL;
	}
	
	// Set the current IPL
	thisCPU->Ipl = newIPL;
	
	KeOnUpdateIPL(newIPL, oldIPL);
	
	// TODO (updated): Issue a self-IPI to call DPCs.
	
	// TODO: call DPCs
	// ideally we'd have something as follows:
	// while (thisCPU->Ipl > newIPL) {
	//     thisCPU->Ipl--;  // lower the IPL by one stage..
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
		pCPU->LapicId    = pInfo->lapic_id;
		pCPU->IsBootstrap    = bIsBSP;
		pCPU->SmpInfo  = pInfo;
		pCPU->PageMapping = KeGetCurrentPageTable(); // it'd better be that
		pCPU->Ipl       = IPL_NOINTS;  // run it at the highest IPL for now. We'll lower it later
		
		pInfo->extra_argument = (uint64_t)pCPU;
		
		// Launch the CPU! Note, we won't actually go there if we are the bootstrap processor.
		AtStore(pInfo->goto_address, &KiCPUBootstrap);
		
		if (bIsBSP)
			pBSPInfo = pInfo;
	}
	
	KiCPUBootstrap(pBSPInfo);
}
