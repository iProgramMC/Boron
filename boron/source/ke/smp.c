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
#include <ldr.h>
#include <ob.h>
#include "ki.h"

KPRCB**  KeProcessorList;
int      KeProcessorCount = 0;
uint32_t KeBootstrapLapicId = 0;

KPRCB** KiGetProcessorList() { return KeProcessorList; }

// TODO: an "init data" section.  Currently there's not much in the data segment
// that can be considered worthy of being purged after system initialization.
KTHREAD  KiExInitThread;

int KeGetProcessorCount()
{
	return KeProcessorCount;
}

uint32_t KeGetBootstrapLapicId()
{
	return KeBootstrapLapicId;
}

extern NO_RETURN void KiInitializePhase1(void* Context);

extern size_t MmTotalAvailablePages;

NO_RETURN void ExpInitializeExecutive(void* Context);

void MmSwitchKernelSpaceLock();

// An atomic write to this field causes the parked CPU to jump to the written address,
// on a 64KiB (or Stack Size Request size) stack. A pointer to the struct limine_smp_info
// structure of the CPU is passed in RDI
NO_RETURN INIT
void KiCPUBootstrap(struct limine_smp_info* pInfo)
{
	PKPRCB Prcb = (PKPRCB)pInfo->extra_argument;
	KeSetCPUPointer(Prcb);
	
	// Update the IPL when initing. Currently we start at the highest IPL
	KeOnUpdateIPL(KeGetIPL(), 0);
	ENABLE_INTERRUPTS();
	
	KeInitCPU();
	KeLowerIPL(IPL_NORMAL);
	
	KeSchedulerInit(MmAllocateKernelStack());
	
	HalInitSystemMP();
	
	if (Prcb->IsBootstrap)
	{
		void* Stack = MmAllocateKernelStack();
		if (!Stack)
			KeCrash("Could not allocate kernel stack for executive init.");
		
		// Spawn a new thread on this CPU that performs initialization
		// of the rest of the kernel.
		KeInitializeThread2(
			&KiExInitThread,
			Stack,
			KERNEL_STACK_SIZE,
			ExpInitializeExecutive,
			NULL,
			KeGetSystemProcess()
		);
		KeSetPriorityThread(&KiExInitThread, PRIORITY_NORMAL);
		KeReadyThread(&KiExInitThread);
	}
	
	// Perform switch to Rwlock for kernel space. This can wait a small amount of time.
	MmSwitchKernelSpaceLock();
	
	KeSchedulerCommit();
}

KPRCB* KeGetCurrentPRCB()
{
	return KeGetCPUPointer();
}

static NO_RETURN void KiCrashedEntry()
{
	KeStopCurrentCPU();
}

extern KSPIN_LOCK KiPrintLock;
extern KSPIN_LOCK KiDebugPrintLock;

bool KiSmpInitted = false;

#ifdef DEBUG
void DbgPrintString(const char* str);
#endif

INIT
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
	
	KiPrintLock.Locked = 0;
	KiDebugPrintLock.Locked = 0;
	
#ifdef DEBUG
	DbgPrintString("\x1B[35m*** Init error: \x1B[0m");
	DbgPrintString(buffer);
#endif
	
	if (HalWasInitted())
	{
		HalDisplayString("\x1B[35m*** Init error: \x1B[0m");
		HalDisplayString(buffer);
	}
	
	if (pSMP)
	{
		for (uint64_t i = 0; i < pSMP->cpu_count; i++)
		{
			struct limine_smp_info* pInfo = pSMP->cpus[i];
			AtStore(pInfo->goto_address, &KiCrashedEntry);
		}
	}
	
	KiCrashedEntry();
}

int KeGetVersionNumber()
{
	return 8;
}

void PsInitSystemProcess();

NO_RETURN INIT
void KeInitSMP()
{
	struct limine_smp_response* pSMP = KeLimineSmpRequest.response;
	struct limine_smp_info* pBSPInfo = NULL;
	
	if (!pSMP)
	{
		KeCrashBeforeSMPInit("Error, a response to the SMP request wasn't provided");
	}
	
	KeBootstrapLapicId = pSMP->bsp_lapic_id;
	
	// TODO, restore to 512. Affinity is a bit mask, and there's no uint512_t. Use a bitmap instead later if you care.
	const uint64_t ProcessorLimit = 64;
	
	if (pSMP->cpu_count > ProcessorLimit)
	{
		KeCrashBeforeSMPInit("Error, unsupported amount of CPUs: %llu (limit is %llu)", pSMP->cpu_count, ProcessorLimit);
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
		InitializeListHead(&prcb->DpcQueue.List);
		
		pInfo->extra_argument = (uint64_t)prcb;
		
		KeProcessorList[i] = prcb;
		
		if (bIsBSP)
			pBSPInfo = pInfo;
	}
	
	// After the below point, KeCrashBeforeSMPInit can't be used, and KeCrash proper doesn't
	// work because the CPUs haven't booted yet
	KiSmpInitted = true;
	
	// Initialize system process.
	PsInitSystemProcess();
	
	int VersionNumber = KeGetVersionNumber();
	LogMsg("Boron (TM), June 2025 - V%d.%02d", VersionNumber / 100, VersionNumber % 100);
	LogMsg("%u System Processors [%u Kb System Memory] MultiProcessor Kernel", pSMP->cpu_count, MmTotalAvailablePages * PAGE_SIZE / 1024);
	
	for (uint64_t i = 0; i < pSMP->cpu_count; i++)
	{
		struct limine_smp_info* pInfo = pSMP->cpus[i];
		
		// Launch the CPU! We won't actually go there immediately if we are the bootstrap processor.
		AtStore(pInfo->goto_address, &KiCPUBootstrap);
	}
	
	KiCPUBootstrap(pBSPInfo);
}
