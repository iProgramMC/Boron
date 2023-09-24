/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/smp.c
	
Abstract:
	This module implements the SMP support for Boron.
	
Author:
	iProgramInCpp - 20 August 2023
***/

#include <ke.h>
#include <mm.h>
#include <hal.h>
#include <arch.h>
#include <string.h>
#include <limreq.h>

KPRCB**  KeProcessorList;
int      KeProcessorCount = 0;
uint32_t KeBootstrapLapicId = 0;

// TODO: Allow grabbing the interrupt code. For simplicity that isn't done
void KeOnUnknownInterrupt(uintptr_t FaultPC)
{
	LogMsg("Unknown interrupt at %p on CPU %u", FaultPC, KeGetCurrentPRCB()->LapicId);
	
	// TODO: crash properly
	KeStopCurrentCPU();
}

void KeOnDoubleFault(uintptr_t FaultPC)
{
	LogMsg("Double fault at %p on CPU %u", FaultPC, KeGetCurrentPRCB()->LapicId);
	
	// TODO: crash properly
	KeStopCurrentCPU();
}

void KeOnProtectionFault(uintptr_t FaultPC)
{
	LogMsg("General Protection Fault at %p on CPU %u", FaultPC, KeGetCurrentPRCB()->LapicId);
	
	// TODO: crash properly
	KeStopCurrentCPU();
}

void KeOnPageFault(uintptr_t FaultPC, uintptr_t FaultAddress, uintptr_t FaultMode)
{
#ifdef DEBUG2
	SLogMsg("handling fault ip=%p, faultaddr=%p, faultmode=%p", FaultPC, FaultAddress, FaultMode);
#endif
	
	if (MmPageFault(FaultPC, FaultAddress, FaultMode))
		return;
	
	KeCrash("unhandled fault ip=%p, faultaddr=%p, faultmode=%p", FaultPC, FaultAddress, FaultMode);
}

#include "../mm/mi.h" // horrible for now

static void KepTestAllMemory(void* Memory, size_t Size)
{
	uint8_t* MemBytes = Memory;
	
	for (size_t i = 0; i < Size; i++)
	{
		MemBytes[i] = i ^ (i << 9) ^ (i << 42) ^ (i << 25);
	}
}

void KiPerformSlabAllocTest()
{
	SLogMsg("Allocating a single uint32_t");
	uint32_t* Memory = MiSlabAllocate(sizeof(uint32_t));
	*Memory = 5;
	LogMsg("Allocated pointer %p of size %d, reading back %u", Memory, sizeof(uint32_t), *Memory);
	
	SLogMsg("Freeing that uint32_t");
	
	MiSlabFree(Memory, sizeof(uint32_t));
	
	SLogMsg("Freed that uint32_t");
	
	SLogMsg("Allocating a bunch of things");
	
	void* Mem16 = MiSlabAllocate(16);
	void* Mem32 = MiSlabAllocate(32);
	void* Mem64 = MiSlabAllocate(64);
	void* Mem128 = MiSlabAllocate(128);
	void* Mem256 = MiSlabAllocate(256);
	void* Mem512 = MiSlabAllocate(512);
	void* Mem1024_1 = MiSlabAllocate(1024);
	void* Mem1024_2 = MiSlabAllocate(1024);
	void* Mem1024_3 = MiSlabAllocate(1024);
	void* Mem1024_4 = MiSlabAllocate(1024);
	void* Mem1024_5 = MiSlabAllocate(1024);
	void* Mem1024_6 = MiSlabAllocate(1024);
	
	SLogMsg("Testing the several allocations");
	
	KepTestAllMemory(Mem16, 16);
	KepTestAllMemory(Mem32, 32);
	KepTestAllMemory(Mem64, 64);
	KepTestAllMemory(Mem128, 128);
	KepTestAllMemory(Mem256, 256);
	KepTestAllMemory(Mem512, 512);
	KepTestAllMemory(Mem1024_1, 1024);
	KepTestAllMemory(Mem1024_2, 1024);
	KepTestAllMemory(Mem1024_3, 1024);
	KepTestAllMemory(Mem1024_4, 1024);
	KepTestAllMemory(Mem1024_5, 1024);
	KepTestAllMemory(Mem1024_6, 1024);
	
	SLogMsg("Freeing everything");
	
	MiSlabFree(Mem16, 16);
	MiSlabFree(Mem32, 32);
	MiSlabFree(Mem64, 64);
	MiSlabFree(Mem128, 128);
	MiSlabFree(Mem256, 256);
	MiSlabFree(Mem512, 512);
	MiSlabFree(Mem1024_1, 1024);
	MiSlabFree(Mem1024_2, 1024);
	MiSlabFree(Mem1024_3, 1024);
	MiSlabFree(Mem1024_4, 1024);
	MiSlabFree(Mem1024_5, 1024);
	MiSlabFree(Mem1024_6, 1024);
	
	SLogMsg("KiPerformSlabAllocTest Done");
}

void KiPerformPageMapTest()
{
	HPAGEMAP pm = KeGetCurrentPageTable();
	
	if (MmMapAnonPage(pm, 0x5ADFDEADB000, MM_PTE_READWRITE | MM_PTE_SUPERVISOR))
	{
		*((uintptr_t*)0x5adfdeadbeef) = 420;
		LogMsg("[CPU %u] Read back from there: %p", KeGetCurrentPRCB()->LapicId, *((uintptr_t*)0x5adfdeadbeef));
		
		// Get rid of that shiza
		MmUnmapPages(pm, 0x5ADFDEADB000, 1);
		
		MmMapAnonPage(pm, 0x5ADFDEADD000, MM_PTE_READWRITE | MM_PTE_SUPERVISOR);
		MmUnmapPages (pm, 0x5ADFDEADD000, 1);
		
		// Try again!  We should get a page fault if we did everything correctly.
		*((uintptr_t*)0x5adfdeadbeef) = 420;
		
		LogMsg("[CPU %u] Read back from there: %p", KeGetCurrentPRCB()->LapicId, *((uintptr_t*)0x5adfdeadbeef));
	}
	else
	{
		LogMsg("Error, failed to map to %p", 0x5ADFDEADB000);
	}
}

// An atomic write to this field causes the parked CPU to jump to the written address,
// on a 64KiB (or Stack Size Request size) stack. A pointer to the struct limine_smp_info
// structure of the CPU is passed in RDI
NO_RETURN void KiCPUBootstrap(struct limine_smp_info* pInfo)
{
	KeSetCPUPointer((KPRCB*)pInfo->extra_argument);
	
	// Update the IPL when initing. Currently we start at the highest IPL
	KeOnUpdateIPL(KeGetIPL(), 0);
	KeSetInterruptsEnabled(true);
	
	KeInitCPU();
	HalMPInit();
	
	KeLowerIPL(IPL_NORMAL);
	
	LogMsg("Hello from CPU %u", (unsigned) pInfo->lapic_id);
	
	KiPerformSlabAllocTest();
	KiPerformPageMapTest();
	
	while (true)
		KeWaitForNextInterrupt();
}

KPRCB* KeGetCurrentPRCB()
{
	return KeGetCPUPointer();
}

static NO_RETURN void KiCrashedEntry()
{
	KeStopCurrentCPU();
}

extern SpinLock g_PrintLock;
extern SpinLock g_DebugPrintLock;

static bool KiSmpInitted = false;

NO_RETURN void KeCrashBeforeSMPInit(const char* message, ...)
{
	if (KiSmpInitted) {
		KeCrash("called KeCrashBeforeSMPInit after SMP init?! (message: %s, RA: %p)", message, __builtin_return_address(0));
	}
	
	struct limine_smp_response* pSMP = KeLimineSmpRequest.response;
	
	// format the message
	va_list va;
	va_start(va, message);
	char buffer[1024]; // may be a beefier message than a garden variety LogMsg
	buffer[sizeof buffer - 3] = 0;
	int chars = vsnprintf(buffer, sizeof buffer - 3, message, va);
	strcpy(buffer + chars, "\n");
	va_end(va);
	
	KeLock(&g_PrintLock);
	HalPrintString("\x1B[35m*** Init error: \x1B[0m");
	HalPrintString(buffer);
	KeUnlock(&g_PrintLock);
	
	KeLock(&g_DebugPrintLock);
	HalPrintStringDebug("\x1B[35m*** Init error: \x1B[0m");
	HalPrintStringDebug(buffer);
	KeUnlock(&g_DebugPrintLock);
	
	for (uint64_t i = 0; i < pSMP->cpu_count; i++)
	{
		struct limine_smp_info* pInfo = pSMP->cpus[i];
		AtStore(pInfo->goto_address, &KiCrashedEntry);
	}
	
	KiCrashedEntry();
}

NO_RETURN void KeInitSMP()
{
	struct limine_smp_response* pSMP = KeLimineSmpRequest.response;
	struct limine_smp_info* pBSPInfo = NULL;
	
	if (!pSMP)
	{
		KeCrashBeforeSMPInit("Error, a response to the SMP request wasn't provided");
	}
	
	KeBootstrapLapicId = pSMP->bsp_lapic_id;
	
	const uint64_t ProcessorLimit = 512;
	
	if (pSMP->cpu_count > ProcessorLimit)
	{
		KeCrashBeforeSMPInit("Error, unsupported amount of CPUs %llu (limit is %llu)", pSMP->cpu_count, ProcessorLimit);
	}
	
	int cpuListPFN = MmAllocatePhysicalPage();
	if (cpuListPFN == PFN_INVALID)
		KeCrashBeforeSMPInit("Error, can't initialize CPU list, we don't have enough memory");
	
	KeProcessorList  = MmGetHHDMOffsetAddr(MmPFNToPhysPage(cpuListPFN));
	KeProcessorCount = pSMP->cpu_count;
	
	// Initialize all the CPUs in series.
	for (uint64_t i = 0; i < pSMP->cpu_count; i++)
	{
		bool bIsBSP = pSMP->bsp_lapic_id == pSMP->cpus[i]->lapic_id;
		
		// Allocate space for the CPU struct.
		int cpuPFN = MmAllocatePhysicalPage();
		if (cpuPFN == PFN_INVALID)
		{
			KeCrashBeforeSMPInit("Error, can't initialize CPUs, we don't have enough memory");
		}
		
		KPRCB* prcb = MmGetHHDMOffsetAddr(MmPFNToPhysPage(cpuPFN));
		memset(prcb, 0, sizeof * prcb);
		
		struct limine_smp_info* pInfo = pSMP->cpus[i];
		
		// initialize the struct
		prcb->Id          = i;
		prcb->LapicId     = pInfo->lapic_id;
		prcb->IsBootstrap = bIsBSP;
		prcb->SmpInfo     = pInfo;
		prcb->Ipl         = IPL_NOINTS;  // run it at the highest IPL for now. We'll lower it later
		
		pInfo->extra_argument = (uint64_t)prcb;
		
		KeProcessorList[i] = prcb;
		
		if (bIsBSP)
			pBSPInfo = pInfo;
	}
	
	KiSmpInitted = true;
	
	for (uint64_t i = 0; i < pSMP->cpu_count; i++)
	{
		struct limine_smp_info* pInfo = pSMP->cpus[i];
		
		// Launch the CPU! We won't actually go there immediately if we are the bootstrap processor.
		AtStore(pInfo->goto_address, &KiCPUBootstrap);
	}
	
	KiCPUBootstrap(pBSPInfo);
}
