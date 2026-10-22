/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ke/fmutex.h

Abstract:
	This header file defines the fast mutex structure and defines the
	interfaces for it.

Author:
	iProgramInCpp - 22 October 2026
***/
#pragma once

typedef struct KEVENT_tag KEVENT, *PKEVENT;

typedef struct
{
	intptr_t Count;
	uintptr_t OwnerCount;
	PKTHREAD OwnerThread;
	KEVENT Event;

#ifdef DEBUG
	uintptr_t Contention;
#endif
}
FAST_MUTEX, *PFAST_MUTEX;

void KeInitializeFastMutex(PFAST_MUTEX Mutex);

void KeAcquireFastMutex(PFAST_MUTEX Mutex);

void KeReleaseFastMutex(PFAST_MUTEX Mutex);

bool KeTryToAcquireFastMutex(PFAST_MUTEX Mutex);
