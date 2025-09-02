/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/mutex.h
	
Abstract:
	This header file contains the definitions for the
	mutex dispatch object.
	
Author:
	iProgramInCpp - 18 November 2023
***/
#pragma once

// The return value of KeReadStateMutex, if the mutex is signaled.
#define MUTEX_SIGNALED (0)

typedef struct KMUTEX_tag
{
	KDISPATCH_HEADER Header;
	
	// Level debugging is performed in debug checked mode.
#ifdef DEBUG
	int Level;
#endif
	
	PKTHREAD OwnerThread;
	
	LIST_ENTRY MutexListEntry;
}
KMUTEX, *PKMUTEX;

void KeInitializeMutex(PKMUTEX Mutex, int Level);

int KeReadStateMutex(PKMUTEX Mutex);

void KeReleaseMutex(PKMUTEX Mutex);

// Releases a mutex.  Unlike KeReleaseMutex, this leaves the dispatcher
// lock locked, and as such, a Wait function (KeWaitForMultipleObjects)
// must be called IMMEDIATELY after this function.
void KeReleaseMutexWait(PKMUTEX Mutex);
