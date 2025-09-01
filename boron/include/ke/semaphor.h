/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/semaphor.h
	
Abstract:
	This header file contains the definitions for the
	semaphore dispatch object.
	
Author:
	iProgramInCpp - 18 November 2023
***/
#pragma once

#include <ke/dispatch.h>

typedef struct KSEMAPHORE_tag
{
	KDISPATCH_HEADER Header;
	
	int Limit;
}
KSEMAPHORE, *PKSEMAPHORE;

#define ASSERT_SEMAPHORE(Semaphore) ASSERT((Semaphore)->Header.Type == DISPATCH_SEMAPHORE)
#define SEMAPHORE_LIMIT_NONE (0x7FFFFFFE)

void KeInitializeSemaphore(PKSEMAPHORE Semaphore, int Count, int Limit);

// Returns the current state of the semaphore.
// If zero, the semaphore is not signaled.
int KeReadStateSemaphore(PKSEMAPHORE Semaphore);

void KeReleaseSemaphore(PKSEMAPHORE Semaphore, int Adjustment, KPRIORITY Increment);

// This function must IMMEDIATELY be followed by a Wait function
// (e.g. KeWaitForSingleObject).  It leaves the dispatcher lock locked.
void KeReleaseSemaphoreWait(PKSEMAPHORE Semaphore, int Adjustment, KPRIORITY Increment);
