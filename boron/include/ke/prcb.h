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
	KSPIN_LOCK TlbsLock;
	
	// architecture specific details
	KARCH_DATA ArchData;
	
	// DPC queue
	KDPC_QUEUE DpcQueue;
	
	// Pending event flags
	int PendingEvents;
	
	// Scheduler for the current processor.
	KSCHEDULER Scheduler;
	
	// HAL Control Block - HAL specific data.
	HALCB_PTR HalData;
}
KPRCB, *PKPRCB;

PKPRCB KeGetCurrentPRCB();

int KeGetProcessorCount();

uint32_t KeGetBootstrapLapicId();

enum
{
	PENDING_YIELD = (1 << 0),
	//PENDING_APCS  = (1 << 1),
	PENDING_DPCS  = (1 << 2),
};

int KeGetPendingEvents();
void KeClearPendingEvents();
void KeSetPendingEvent(int PendingEvent);
void KeIssueSoftwareInterrupt();
void KeIssueSoftwareInterruptApcLevel();

static_assert(sizeof(KPRCB) <= 4096, "struct KPRCB should be smaller or equal to the page size, for objective reasons");

#endif // BORON_KE_PRCB_H
