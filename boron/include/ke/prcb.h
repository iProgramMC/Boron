/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/prcb.h
	
Abstract:
	This module contains the structure definitions for
	the PRCB (PRocessor Control Block)
	
Author:
	iProgramInCpp - 22 September 2023
***/
#ifndef BORON_KE_PRCB_H
#define BORON_KE_PRCB_H

#include <arch.h>
#include <_limine.h>

#include <hal/data.h>

#include <ke/ipl.h>
#include <ke/dpc.h>
#include <ke/sched.h>

NO_RETURN void KeStopCurrentCPU(void); // stops the current CPU

typedef struct KPRCB_tag
{
	// TLB shootdown information.
	// Moved to offset 0 because it's hardcoded by KiHandleTlbShootdownIpiA on AMD64.
	
	// Address - the address where the TLB shootdown process will start
	uintptr_t TlbsAddress;
	// Length - the number of pages the TLB shootdown handler will invalidate
	size_t    TlbsLength;
	// Lock - Used to synchronize TLB shootdown calls.
	// - First it is locked by the TLB shootdown emitter.
	// - Then the same core tries to lock it again, waiting until the receiver
	//   of the TLB shootdown unlocks this lock.
	// - In the TLB shootdown handler, this lock is unlocked, letting other TLB
	//   shootdown requests come in.
	KSPIN_LOCK TlbsLock;
	
	// NOTE: This is supposed to be at offset 0x18.  Update ke/amd64/trap.asm if you need this moved.
	uintptr_t SysCallStack;
	
	// the index of the processor within the KeProcessorList
	int Id;
	
	// the APIC ID of the processor
	uint32_t LapicId;
	
	// are we the bootstrap processor?
	bool IsBootstrap;
	
	// the SMP info we're given
	struct limine_smp_info* SmpInfo;
	
	// the current IPL that we are running at
	KIPL Ipl;
	
	// architecture specific details
	KARCH_DATA ArchData;
	
	// DPC queue
	KDPC_QUEUE DpcQueue;
	
	// Whether a yield is pending for the current thread.
	bool YieldCurrentThread;
	
	// Software interrupt pending flags.
	#define PENDING(Ipl) (1 << (Ipl - 1))
	int PendingSoftInterrupts;
	
	// Scheduler for the current processor.
	KSCHEDULER Scheduler;
	
	// HAL Control Block - HAL specific data.
	PKHALCB HalData;
}
KPRCB, *PKPRCB;

#ifdef TARGET_AMD64

ALWAYS_INLINE static inline
KIPL KeGetIPL()
{
	KIPL Result;
	ASM("mov %%gs:%c1, %0" : "=r" (Result) : "i" (offsetof(KPRCB, Ipl)));
	return Result;
}

ALWAYS_INLINE static inline
PKTHREAD KeGetCurrentThread()
{
	PKTHREAD Thread;
	ASM("mov %%gs:%c1, %0" : "=r" (Thread) : "i" (offsetof(KPRCB, Scheduler.CurrentThread)));
	return Thread;
}

#else

// Get the current IPL of the processor.
KIPL KeGetIPL();

// Get the pointer to the current thread.
PKTHREAD KeGetCurrentThread();

#endif

PKPRCB KeGetCurrentPRCB();

int KeGetProcessorCount();

uint32_t KeGetBootstrapLapicId();

static_assert(sizeof(KPRCB) <= 4096, "struct KPRCB should be smaller or equal to the page size, for objective reasons");

#endif // BORON_KE_PRCB_H
