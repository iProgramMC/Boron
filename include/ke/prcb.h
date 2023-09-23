/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/crash.c
	
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

NO_RETURN void KeStopCurrentCPU(void); // stops the current CPU

typedef struct KPRCB_tag
{
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
	
	// TLB shootdown information.
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
	SpinLock  TlbsLock;
	
	// architecture specific details
	KARCH_DATA ArchData;
}
KPRCB;

KPRCB* KeGetCurrentPRCB();

static_assert(sizeof(KPRCB) <= 4096, "struct KPRCB should be smaller or equal to the page size, for objective reasons");

#endif // BORON_KE_PRCB_H
