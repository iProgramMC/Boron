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

void KeInitializeEvent(PKEVENT Event, int EventType, bool State);

bool KeReadStateEvent(PKEVENT Event);

void KeSetEvent(PKEVENT Event);

void KeResetEvent(PKEVENT Event);

void KePulseEvent(PKEVENT Event);
