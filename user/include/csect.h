/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	csect.h
	
Abstract:
	This header defines the APIs for Boron OSDLL's
	critical section object.
	
Author:
	iProgramInCpp - 15 July 2025
***/
#pragma once

#include <main.h>
#include <handle.h>

// This structure must be handled as logically opaque by user code.
typedef struct
{
	// Is it locked?
	int Locked;

	// Amount of times to spin on Locked before deferring to a system call
	int MaxSpins;

	HANDLE EventHandle;
}
OS_CRITICAL_SECTION, *POS_CRITICAL_SECTION;

// Initializes a critical section.
BSTATUS OSInitializeCriticalSection(POS_CRITICAL_SECTION CriticalSection);

// Initializes a critical section and .
BSTATUS OSInitializeCriticalSectionWithSpinCount(POS_CRITICAL_SECTION CriticalSection, int MaxSpins);

// Deletes a critical section.
void OSDeleteCriticalSection(POS_CRITICAL_SECTION CriticalSection);

// Attempts to enter a critical section.  Returns, as a boolean,
// whether or not the entry into the critical section succeeded.
bool OSTryEnterCriticalSection(POS_CRITICAL_SECTION CriticalSection);

// Enters a critical section.
void OSEnterCriticalSection(POS_CRITICAL_SECTION CriticalSection);

// Leaves a critical section.
void OSLeaveCriticalSection(POS_CRITICAL_SECTION CriticalSection);
