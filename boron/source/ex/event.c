/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/event.c
	
Abstract:
	This module implements the Event executive dispatch object.
	
Author:
	iProgramInCpp - 12 August 2024
***/
#include "exp.h"

POBJECT_TYPE
	ExEventObjectType,
	ExSemaphoreObjectType,
	ExTimerObjectType,
	ExThreadObjectType,
	ExProcessObjectType;

bool ExpCreateEventType()
{
	OBJECT_TYPE_INFO ObjectInfo;
	memset (&ObjectInfo, 0, sizeof ObjectInfo);
	
	ObjectInfo.NonPagedPool = true;
	ObjectInfo.MaintainHandleCount = true;
	
	BSTATUS Status = ObCreateObjectType(
		"Event",
		&ObjectInfo,
		&ExEventObjectType
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Could not create Event type: %d", Status);
		return false;
	}
	
	return true;
}

BSTATUS OSSetEvent(HANDLE EventHandle)
{
	BSTATUS Status;
	void* EventV;
	
	Status = ObReferenceObjectByHandle(EventHandle, ExEventObjectType, &EventV);
	
	if (SUCCEEDED(Status))
	{
		KeSetEvent((PKEVENT) EventV, EX_DISPATCH_BOOST);
		ObDereferenceObject(EventV);
	}
	
	return Status;
}

BSTATUS OSResetEvent(HANDLE EventHandle)
{
	BSTATUS Status;
	void* EventV;
	
	Status = ObReferenceObjectByHandle(EventHandle, ExEventObjectType, &EventV);
	
	if (SUCCEEDED(Status))
	{
		KeResetEvent((PKEVENT) EventV);
		ObDereferenceObject(EventV);
	}
	
	return Status;
}

BSTATUS OSPulseEvent(HANDLE EventHandle)
{
	BSTATUS Status;
	void* EventV;
	
	Status = ObReferenceObjectByHandle(EventHandle, ExEventObjectType, &EventV);
	
	if (SUCCEEDED(Status))
	{
		KePulseEvent((PKEVENT) EventV, EX_DISPATCH_BOOST);
		ObDereferenceObject(EventV);
	}
	
	return Status;
}

BSTATUS OSQueryEvent(HANDLE EventHandle, int* EventState)
{
	if (!EventState)
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status;
	void* EventV;
	
	Status = MmProbeAddress(EventState, sizeof(int), true, KeGetPreviousMode());
	if (FAILED(Status))
		return Status;
	
	Status = ObReferenceObjectByHandle(EventHandle, ExMutexObjectType, &EventV);
	
	if (SUCCEEDED(Status))
	{
		int State = KeReadStateEvent((PKEVENT) EventV);
		Status = MmSafeCopy(EventState, &State, sizeof(int), KeGetPreviousMode(), true);
		ObDereferenceObject(EventV);
	}
	
	return Status;
}

typedef struct {
	int EventType;
	bool State;
}
EVENT_DATA, *PEVENT_DATA;

static BSTATUS ExpInitializeEventObject(void* EventV, void* Context)
{
	PEVENT_DATA EventData = Context;
	KeInitializeEvent((PKEVENT) EventV, EventData->EventType, EventData->State);
	return STATUS_SUCCESS;
}

BSTATUS OSCreateEvent(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, int EventType, bool State)
{
	// Check if the event type is within parameters.
	if (EventType != EVENT_SYNCHRONIZATION && EventType != EVENT_NOTIFICATION)
		return STATUS_INVALID_PARAMETER;
	
	EVENT_DATA EventData;
	EventData.EventType = EventType;
	EventData.State     = State;
	
	return ExiCreateObjectUserCall(OutHandle, ObjectAttributes, ExEventObjectType, sizeof(KEVENT), ExpInitializeEventObject, &EventData);
}

BSTATUS OSOpenEvent(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, int OpenFlags)
{
	return ExiOpenObjectUserCall(OutHandle, ObjectAttributes, ExEventObjectType, OpenFlags);
}
