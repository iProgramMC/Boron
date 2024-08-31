/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/thread.c
	
Abstract:
	This module implements the process manager thread
	object in Boron.
	
Author:
	iProgramInCpp - 30 August 2024
***/
#include "psp.h"

void PspDeleteThread(void* ThreadV)
{
	UNUSED PETHREAD Thread = ThreadV;
	
	// TODO: Free thread's stack, etc.
	
	DbgPrint("TODO: PspDeleteThread");
}

INIT
bool PsCreateThreadType()
{
	OBJECT_TYPE_INFO ObjectInfo;
	memset (&ObjectInfo, 0, sizeof ObjectInfo);
	
	ObjectInfo.NonPagedPool = true;
	ObjectInfo.MaintainHandleCount = true;
	
	ObjectInfo.Delete = PspDeleteThread;
	
	BSTATUS Status = ObCreateObjectType(
		"Thread",
		&ObjectInfo,
		&PsThreadObjectType
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Could not create Thread type: %d", Status);
		return false;
	}
	
	return true;
}

BSTATUS PsCreateSystemThread(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	HANDLE ProcessHandle,
	PKTHREAD_START StartRoutine,
	void* StartContext)
{
	return STATUS_UNIMPLEMENTED;
}
