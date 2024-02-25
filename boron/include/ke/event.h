/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/event.h
	
Abstract:
	This header file contains the definitions for the
	event dispatch object.
	
Author:
	iProgramInCpp - 11 November 2023
***/
#pragma once

enum // [1]
{
	EVENT_SYNCHRONIZATION,
	EVENT_NOTIFICATION,
};

typedef struct KEVENT_tag
{
	KDISPATCH_HEADER Header;
	
	int Type; // [1]
}
KEVENT, *PKEVENT;

#define ASSERT_EVENT(Event) ASSERT((Event)->Header.Type == DISPATCH_EVENT)

void KeInitializeEvent(PKEVENT Event, int EventType, bool State);

bool KeReadStateEvent(PKEVENT Event);

void KeSetEvent(PKEVENT Event, KPRIORITY Increment);

void KeResetEvent(PKEVENT Event);

void KePulseEvent(PKEVENT Event, KPRIORITY Increment);

#ifdef KERNEL
// Don't use.
//
// This API is supposed to be called at IPL_DPC and above. Doing so at a lower
// IPL could cause a crash in debug mode, or a potentially invalid thread pointer
// in release mode.
//
// Good thing that this API is only used in the rwlock implementation.
PKTHREAD KeSetEventAndGetWaiter(PKEVENT Event, KPRIORITY Increment);
#endif
