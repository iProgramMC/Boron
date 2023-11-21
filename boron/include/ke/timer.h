/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/timer.h
	
Abstract:
	This header file contains the definitions for the
	timer dispatch object.
	
Author:
	iProgramInCpp - 30 October 2023
***/
#pragma once

#include <ke/dispatch.h>

typedef struct KTIMER_tag
{
	KDISPATCH_HEADER Header;
	
	AATREE_ENTRY EntryTree; // Entry in the global timer tree
	
	uint64_t ExpiryTick;   // Time at which the timer expires
	
	bool IsEnqueued;
	
	PKDPC Dpc; // DPC to be enqueued when timer is due
}
KTIMER, *PKTIMER;

#define ASSERT_TIMER(Timer) ASSERT((Timer)->Header.Type == DISPATCH_TIMER)

void KeInitializeTimer(PKTIMER Timer);

bool KeCancelTimer(PKTIMER Timer);

bool KeReadStateTimer(PKTIMER Timer);

bool KeSetTimer(PKTIMER Timer, uint64_t DueTimeMs, PKDPC Dpc);

#ifdef KERNEL
// Internal version of KiSetTimer that must be run with the dispatcher
// locked (such as in a DPC, or in an internal dispatcher function)
bool KiSetTimer(PKTIMER Timer, uint64_t DueTimeMs, PKDPC Dpc);

// Ditto with KeCancelTimer.
bool KiCancelTimer(PKTIMER Timer);

#endif
