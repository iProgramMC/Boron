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
	
	LIST_ENTRY EntryQueue; // Entry in the global timer queue
	
	uint64_t ExpiryTick;   // Time at which the timer expires
	
	bool IsEnqueued;
}
KTIMER, *PKTIMER;

void KeInitializeTimer(PKTIMER Timer);

bool KeCancelTimer(PKTIMER Timer);

bool KeReadStateTimer(PKTIMER Timer);

bool KeSetTimer(PKTIMER Timer, uint64_t DueTimeMs);

uint64_t KeGetSoonestTimerExpiry();

uint64_t KeGetNextTimerExpiryDurationInItTicks();

void KiDispatchTimerObjects(); // Called by the scheduler
