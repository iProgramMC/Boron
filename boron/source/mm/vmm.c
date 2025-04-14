/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/pmm.c
	
Abstract:
	This module contains the implementation for the virtual
	memory manager in Boron. This currently includes the
	TLB shootdown request thunk and the page fault "handler".
	
Author:
	iProgramInCpp - 28 August 2023
***/

#include "mi.h"
#include <ke.h>
#include <ex.h>

static KIPL MmpKernelSpaceSpinLockIpl;
static KSPIN_LOCK MmpKernelSpaceSpinLock;
static EX_RW_LOCK MmpKernelSpaceRwLock;

// spinlock version used during init
static void MmpLockKernelSpace_Spin()
{
	KIPL Ipl;
	KeAcquireSpinLock(&MmpKernelSpaceSpinLock, &Ipl);
	MmpKernelSpaceSpinLockIpl = Ipl;
}
static void MmpUnlockKernelSpace_Spin()
{
	KeReleaseSpinLock(&MmpKernelSpaceSpinLock, MmpKernelSpaceSpinLockIpl);
}
// rwlock version
static void MmpLockKernelSpaceShared_Rw()
{
	BSTATUS Status = ExAcquireSharedRwLock(&MmpKernelSpaceRwLock, false, false, false);
	ASSERT(Status == STATUS_SUCCESS);
}
static void MmpLockKernelSpaceExclusive_Rw()
{
	BSTATUS Status = ExAcquireSharedRwLock(&MmpKernelSpaceRwLock, false, false, false);
	ASSERT(Status == STATUS_SUCCESS);
}
static void MmpUnlockKernelSpace_Rw()
{
	ExReleaseRwLock(&MmpKernelSpaceRwLock);
}

static void(*MmpLockKernelSpaceSharedFunc)()    = MmpLockKernelSpace_Spin;
static void(*MmpLockKernelSpaceExclusiveFunc)() = MmpLockKernelSpace_Spin;
static void(*MmpUnlockKernelSpaceFunc)()        = MmpUnlockKernelSpace_Spin;

void MmLockKernelSpaceShared()
{
	MmpLockKernelSpaceSharedFunc();
}

void MmLockKernelSpaceExclusive()
{
	MmpLockKernelSpaceExclusiveFunc();
}

void MmUnlockKernelSpace()
{
	MmpUnlockKernelSpaceFunc();
}

static volatile int MmSyncLockSwitch;
static volatile int MmSyncLockSwitch2;

// Called from ExpInitializeExecutive.
INIT
void MmSwitchKernelSpaceLock()
{
	extern int KeProcessorCount;
	int ProcCount = KeProcessorCount;
	PKPRCB Prcb = KeGetCurrentPRCB();
	
	// Wait until all processors end up here
	AtAddFetch(MmSyncLockSwitch, 1);
	while (AtLoad(MmSyncLockSwitch) < ProcCount)
		KeSpinningHint();
	
	// All processors are here, if we're the bootstrap processor, perform the switch!!
	if (Prcb->IsBootstrap)
	{
		ExInitializeRwLock(&MmpKernelSpaceRwLock);
		MmpLockKernelSpaceSharedFunc    = MmpLockKernelSpaceShared_Rw;
		MmpLockKernelSpaceExclusiveFunc = MmpLockKernelSpaceExclusive_Rw;
		MmpUnlockKernelSpaceFunc        = MmpUnlockKernelSpace_Rw;
	}
	
	// Done, now sync again
	//
	// TODO: Not re-using the original MmSyncLockSwitch variable.  It seems I have issues booting on
	// one of my pieces of hardware due to it... I don't know why it works now.
	AtAddFetch(MmSyncLockSwitch2, 2);
	while (AtLoad(MmSyncLockSwitch2) < ProcCount)
		KeSpinningHint();
}

// forces all cores to issue a TLB shootdown (invalidate the address from the
// TLB - with invlpg on amd64 for instance)
void MmIssueTLBShootDown(uintptr_t Address, size_t Length)
{
	KeIssueTLBShootDown(Address, Length);
}

HPAGEMAP MiGetCurrentPageMap()
{
	return (HPAGEMAP) KeGetCurrentPageTable();
}

void MiInitPoolEntryAllocator();

INIT
void MmInitAllocators()
{
	MiInitPoolEntryAllocator();
	MiInitPool();
	MiInitSlabs();
}

KIPL MmLockSpaceShared(uintptr_t DecidingAddress)
{
	KIPL Ipl = KeRaiseIPL(IPL_APC);
	
	if (MM_KERNEL_SPACE_BASE <= DecidingAddress)
	{
		MmLockKernelSpaceShared();
	}
	else
	{
		PEX_RW_LOCK Lock = &PsGetAttachedProcess()->AddressLock;
		BSTATUS Status = ExAcquireSharedRwLock(Lock, false, false, false);
		ASSERT(Status == STATUS_SUCCESS);
	}
	
	return Ipl;
}

KIPL MmLockSpaceExclusive(uintptr_t DecidingAddress)
{
	KIPL Ipl = KeRaiseIPL(IPL_APC);
	
	if (MM_KERNEL_SPACE_BASE <= DecidingAddress)
	{
		MmLockKernelSpaceExclusive();
	}
	else
	{
		PEX_RW_LOCK Lock = &PsGetAttachedProcess()->AddressLock;
		BSTATUS Status = ExAcquireExclusiveRwLock(Lock, false, false);
		ASSERT(Status == STATUS_SUCCESS);
	}
	
	return Ipl;
}

void MmUnlockSpace(KIPL Ipl, uintptr_t DecidingAddress)
{
	if (MM_KERNEL_SPACE_BASE <= DecidingAddress)
		MmUnlockKernelSpace();
	else
		ExReleaseRwLock(&PsGetAttachedProcess()->AddressLock);
	
	KeLowerIPL(Ipl);
}
