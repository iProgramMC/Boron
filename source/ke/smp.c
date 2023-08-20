// Boron64 - SMP initialization
#include <ke.h>
#include <mm.h>
#include <hal.h>
#include <string.h>
#include <_limine.h>

volatile struct limine_smp_request g_SMPRequest =
{
	.id = LIMINE_SMP_REQUEST,
	.revision = 0,
	.response = NULL,
	.flags = 0,
};

// An atomic write to this field causes the parked CPU to jump to the written address,
// on a 64KiB (or Stack Size Request size) stack. A pointer to the struct limine_smp_info
// structure of the CPU is passed in RDI
NO_RETURN void KiCPUBootstrap(struct limine_smp_info* pInfo)
{
	LogMsg("Hello from CPU %u", (unsigned) pInfo->lapic_id);
	
	KeStopCurrentCPU();
}

NO_RETURN void KeInitSMP()
{
	struct limine_smp_response* pSMP = g_SMPRequest.response;
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
