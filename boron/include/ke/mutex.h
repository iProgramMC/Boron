/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/mutex.h
	
Abstract:
	This header file contains the definitions for the
	mutex (mutual exclusion) dispatch object.
	
Author:
	iProgramInCpp - 31 October 2023
***/
#pragma once

#include <ke/dispatch.h>

typedef struct KMUTEX_tag
{
	KDISPATCH_HEADER Header;
	
	// Which thread currently owns the mutex.
	PKTHREAD Owner;
	
	// Ticket lock used to control access to the mutex
	KTICKET_LOCK SpinLock;
}
KMUTEX, *PKMUTEX;

void KeAcquireMutex(PKMUTEX Mutex);

void KeReleaseMutex(PKMUTEX Mutex);
