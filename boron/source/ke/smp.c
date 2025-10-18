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

PLOADER_PARAMETER_BLOCK KeGetLoaderParameterBlock()
{
	return &KeLoaderParameterBlock;
}

const char* KeGetBootCommandLine()
{
	return KeLoaderParameterBlock.CommandLine;
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
void KiCPUBootstrap(PLOADER_AP LoaderAp)
{
	PKPRCB Prcb = LoaderAp->ExtraArgument;
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
	
	// format the message
	va_list va;
	va_start(va, message);
	static char buffer[2048]; // may be a beefier message than a garden variety LogMsg
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
	
	PLOADER_MP_INFO MpInfo = &KeLoaderParameterBlock.Multiprocessor;
	for (uint64_t i = 0; i < MpInfo->Count; i++)
		KeMarkCrashedAp(i);
	
	DbgPrintStackTrace(0);
	KeStopCurrentCPU();
}

void PsInitSystemProcess();

#ifndef CONFIG_SMP
static KPRCB KiPrcb;
static PKPRCB KiPrcbList[1];
#endif

NO_RETURN INIT
void KeInitSMP()
{
#ifdef CONFIG_SMP
	#define UNI_OR_MULTI "Multi"
#else
	#define UNI_OR_MULTI "Uni"
#endif

	PLOADER_MP_INFO MpInfo = &KeLoaderParameterBlock.Multiprocessor;
	PLOADER_AP BspAp = NULL;
	
	KeBootstrapLapicId = MpInfo->BootstrapHardwareId;
	
	// TODO, restore to 512. Affinity is a bit mask, and there's no uint512_t. Use a bitmap instead later if you care.
	const uint64_t ProcessorLimit = 64;
	
	if (MpInfo->Count > ProcessorLimit)
		KeCrashBeforeSMPInit("Error, unsupported amount of CPUs: %llu (limit is %llu)", MpInfo->Count, ProcessorLimit);
	
#ifdef CONFIG_SMP
	int cpuListPFN = MmAllocatePhysicalPage();
	if (cpuListPFN == PFN_INVALID)
		KeCrashBeforeSMPInit("Error, can't initialize CPU list, we don't have enough memory");
	
	// TODO: don't use MmGetHHDMOffsetAddr on 32-bit builds?
	KeProcessorList  = MmGetHHDMOffsetAddr(MmPFNToPhysPage(cpuListPFN));
	KeProcessorCount = MpInfo->Count;
#else
	KeProcessorList  = KiPrcbList;
	KeProcessorCount = 1;
	
	if (MpInfo->Count >= 1)
	{
		DbgPrint("%zu processors provided by the loader, but we only support one");
		MpInfo->Count = 1;
	}
#endif
	
	// Initialize all the CPUs in series.
	for (uint64_t i = 0; i < MpInfo->Count; i++)
	{
		PLOADER_AP LoaderAp = &MpInfo->List[i];
		bool bIsBSP = KeBootstrapLapicId == LoaderAp->HardwareId;
		
		// Allocate space for the CPU struct.
		int PrcbPfn = MmAllocatePhysicalPage();
		if (PrcbPfn == PFN_INVALID)
		{
			KeCrashBeforeSMPInit("Error, can't initialize CPUs, we don't have enough memory");
		}
		
	#ifdef CONFIG_SMP
		PKPRCB Prcb = MmGetHHDMOffsetAddr(MmPFNToPhysPage(PrcbPfn));
	#else
		PKPRCB Prcb = &KiPrcb;
		ASSERT(MpInfo->Count == 1);
	#endif
		memset(Prcb, 0, sizeof *Prcb);
		
		// initialize the struct
		Prcb->Id          = i;
		Prcb->LapicId     = LoaderAp->HardwareId;
		Prcb->IsBootstrap = bIsBSP;
		Prcb->LoaderAp    = LoaderAp;
		Prcb->Ipl         = IPL_NOINTS;  // run it at the highest IPL for now. We'll lower it later
		InitializeListHead(&Prcb->DpcQueue.List);
		
		LoaderAp->ExtraArgument = Prcb;
		KeProcessorList[i] = Prcb;
		
		if (bIsBSP)
			BspAp = LoaderAp;
	}
	
	// After the below point, KeCrashBeforeSMPInit can't be used, and KeCrash proper doesn't
	// work because the CPUs haven't booted yet
	KiSmpInitted = true;
	
	// Initialize system process.
	PsInitSystemProcess();
	
	int VersionNumber = KeGetVersionNumber();
	LogMsg("Boron (TM), October 2025 - v%d.%d.%d (%s)", VER_MAJOR(VersionNumber), VER_MINOR(VersionNumber), VER_BUILD(VersionNumber), BORON_TARGET);
	LogMsg("%u System Processors [%u Kb System Memory] " UNI_OR_MULTI "Processor Kernel", MpInfo->Count, MmTotalAvailablePages * PAGE_SIZE / 1024);
	
	for (uint64_t i = 0; i < MpInfo->Count; i++)
	{
		// Launch the CPU! We won't actually go there immediately if we are the bootstrap processor.
		KeJumpstartAp(i);
	}
	
	KiCPUBootstrap(BspAp);
}
