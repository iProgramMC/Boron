/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	borondll/src/csect.c
	
Abstract:
	This module implements the OSDLL's critical section object.
	
Author:
	iProgramInCpp - 15 July 2025
***/
#include <boron.h>
#include <atom.h>

// If this option is set to 1, then multiple waiters wake up at the same time
// when a thread releases a critical section.  This can improve performance in
// some applications or diminish it in others.  It has to be benchmarked.
#define MULTIPLE_WAITERS_WAKEUP 0

#define DEFAULT_MAX_SPINS 1024

BSTATUS OSInitializeCriticalSectionWithSpinCount(POS_CRITICAL_SECTION CriticalSection, int MaxSpins)
{
	CriticalSection->Locked = 0;
	CriticalSection->MaxSpins = MaxSpins;

	OBJECT_ATTRIBUTES Attributes = {};
	Attributes.OpenFlags = OB_OPEN_DUPLICATE_ON_FORK;

	BSTATUS Status = OSCreateEvent(
		&CriticalSection->EventHandle,
		&Attributes,
		MULTIPLE_WAITERS_WAKEUP ? EVENT_NOTIFICATION : EVENT_SYNCHRONIZATION,
		false
	);

	return Status;
}

BSTATUS OSInitializeCriticalSection(POS_CRITICAL_SECTION CriticalSection)
{
	return OSInitializeCriticalSectionWithSpinCount(CriticalSection, DEFAULT_MAX_SPINS);
}

void OSDeleteCriticalSection(POS_CRITICAL_SECTION CriticalSection)
{
	OSClose(CriticalSection->EventHandle);
	
	CriticalSection->EventHandle = HANDLE_NONE;
}

static inline ALWAYS_INLINE
bool OSTryEnterCriticalSection_(POS_CRITICAL_SECTION CriticalSection)
{
	int Expected = 0;
	if (AtCompareExchange(&CriticalSection->Locked, &Expected, 1))
	{
	#if MULTIPLE_WAITERS_WAKEUP
		OSResetEvent(CriticalSection->EventHandle);
	#endif
		return true;
	}

	return false;
}

bool OSTryEnterCriticalSection(POS_CRITICAL_SECTION CriticalSection)
{
	return OSTryEnterCriticalSection_(CriticalSection);
}

void OSEnterCriticalSection(POS_CRITICAL_SECTION CriticalSection)
{
	for (int i = 0; i < CriticalSection->MaxSpins; i++)
	{
		if (OSTryEnterCriticalSection_(CriticalSection))
			return;

		SpinHint();
	}

	while (true)
	{
		if (OSTryEnterCriticalSection_(CriticalSection))
			return;

		// TODO: Handle failure cases here.
		OSWaitForSingleObject(CriticalSection->EventHandle, false, WAIT_TIMEOUT_INFINITE);
	}
}

void OSLeaveCriticalSection(POS_CRITICAL_SECTION CriticalSection)
{
	AtStore(CriticalSection->Locked, 0);
	OSSetEvent(CriticalSection->EventHandle);
}
