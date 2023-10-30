/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/dispatch.h
	
Abstract:
	This header file contains the definitions for the
	dispatch objects.
	
Author:
	iProgramInCpp - 30 October 2023
***/
#pragma once

#include <rtl/list.h>

typedef struct KTHREAD_tag KTHREAD, *PKTHREAD;

typedef struct KDISPATCH_HEADER_tag
{
	uint8_t Type;
	uint8_t Signaled;
	uint16_t Spare1;
	uint32_t Spare2;
	
	LIST_ENTRY WaitBlockList;
}
KDISPATCH_HEADER, *PKDISPATCH_HEADER;

enum
{
	DISPOBJ_EVENT,
	DISPOBJ_TIMER,
	DISPOBJ_MUTEX,
	DISPOBJ_SEMAPHORE,
	DISPOBJ_THREAD,
	DISPOBJ_PROCESS,
};

enum
{
	WAIT_TYPE_ALL,
	WAIT_TYPE_ANY,
};

typedef struct KWAIT_BLOCK_tag
{
	LIST_ENTRY        Entry;   // Entry into the linked list of threads waiting for an object.
	PKDISPATCH_HEADER Object;  // The object that the containing thread waits on.
	PKTHREAD          Thread;  // The thread that the wait block is part of.
}
KWAIT_BLOCK, *PKWAIT_BLOCK;

// Wait for multiple objects at once.
int KeWaitForMultipleObjects(
	int Count,
	PKDISPATCH_HEADER Objects[],
	int WaitType,
	bool Alertable,
	PKWAIT_BLOCK WaitBlockArray);

// Initialize the dispatcher header.
void KeInitializeDispatchHeader(PKDISPATCH_HEADER Object);

// Wait for a single object.
int KeWaitForSingleObject(PKDISPATCH_HEADER Object, bool Alertable);

// Set an object to the signaled state, and alert the waiting threads about it.
void KeSignalObject(PKDISPATCH_HEADER Object);

// Dispatch objects
#include "timer.h"
#include "mutex.h"
