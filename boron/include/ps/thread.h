/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/thread.h
	
Abstract:
	This header file defines the ETHREAD structure, which is an
	extension of KTHREAD, and is exposed by the object manager.
	
Author:
	iProgramInCpp - 26 November 2023
***/
#pragma once

#include <ke/thread.h>

typedef struct ETHREAD_tag
{
	KTHREAD Tcb;
	
	PEPROCESS Process;
	
	void* UserStack;
	
	size_t UserStackSize;
	
	bool Initialized;
}
ETHREAD, *PETHREAD;

#define PsGetCurrentThread() ((PETHREAD) KeGetCurrentThread())

BSTATUS PsCreateSystemThreadFast(
	PETHREAD* OutThread,
	PKTHREAD_START StartRoutine,
	void* StartContext,
	bool CreateSuspended
);

BSTATUS PsCreateSystemThread(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	HANDLE ProcessHandle,
	PKTHREAD_START StartRoutine,
	void* StartContext,
	bool CreateSuspended
);

BSTATUS OSCreateThread(
	PHANDLE OutHandle,
	HANDLE ProcessHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PKTHREAD_START ThreadStart,
	void* ThreadContext,
	bool CreateSuspended
);

BSTATUS OSTerminateThread(HANDLE Handle);

BSTATUS OSSetSuspendedThread(HANDLE Handle, bool IsSuspended);

BSTATUS OSSleep(int Milliseconds);

// TODO: Need I do anything more?
#define PsTerminateThread() KeTerminateThread(1)
