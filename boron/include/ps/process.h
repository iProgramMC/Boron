/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/process.h
	
Abstract:
	This header file defines the EPROCESS structure, which is an
	extension of KPROCESS, and is exposed by the object manager.
	
Author:
	iProgramInCpp - 26 November 2023
***/
#pragma once

#include <ex/process.h>

typedef struct _OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

PEPROCESS PsGetSystemProcess();

// Gets the currently attached process, or if there is no
// attached process, the currently running process.
PEPROCESS PsGetAttachedProcess();

void PsInitSystemProcess();

bool PsInitSystem();

bool PsInitSystemPart2();

void PsAttachToProcess(PEPROCESS Process);

void PsDetachFromProcess();

BSTATUS OSCreateProcess(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	HANDLE ParentProcessHandle,
	bool InheritHandles,
	bool DeepCloneHandles
);

// POSIX fork support.
BSTATUS OSForkProcess(PHANDLE OutChildHandle, void* ChildReturnPC, void* ChildReturnSP);

#ifdef KERNEL
extern EPROCESS PsSystemProcess;
#else
#define PsSystemProcess (*PsGetSystemProcess())
#endif
