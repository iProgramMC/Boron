/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/process.h
	
Abstract:
	This header file defines the kernel process object
	and its manipulation functions.
	
Author:
	iProgramInCpp - 22 November 2023
***/
#pragma once

#include <ke/dispatch.h>
#include <mm.h>

typedef struct KPROCESS_tag KPROCESS, *PKPROCESS;

typedef void(*PKPROCESS_TERMINATE_METHOD)(PKPROCESS Process);

struct KPROCESS_tag
{
	// Dispatch object header.
	KDISPATCH_HEADER Header;
	
	// Pointer to common address map
	HPAGEMAP PageMap;
	
	// List of child threads
	LIST_ENTRY ThreadList;
	
	// Total accumulated time for all threads
	uint64_t AccumulatedTime;
	
	// Default thread priority
	int DefaultPriority; 
	
	// Default thread affinity
	KAFFINITY DefaultAffinity;
	
	// Whether the process was detached or not.
	bool Detached;
	
	// Method called when the process terminates, in detached mode.
	// The method is called at IPL_DPC with the dispatcher database
	// locked.
	PKPROCESS_TERMINATE_METHOD TerminateMethod;
};

// Allocate an uninitialized process instance.  Use when you want to detach the process.
PKPROCESS KeAllocateProcess();

// Get the system process.
PKPROCESS KeGetSystemProcess();

// Deallocate a process allocated with KeAllocateProcess.
void KeDeallocateProcess(PKPROCESS Process);

// Initialize the process.
void KeInitializeProcess(PKPROCESS Process, int BasePriority, KAFFINITY BaseAffinity);

// Attach to a process' address space.
void KeAttachToProcess(PKPROCESS Process);

// Detach from the attached process' address space.
void KeDetachFromProcess();

// Detaches a child process from the managing process. This involves automatic cleanup
// through the terminate method specified in the respective function parameter.
//
// Notes:
// - If TerminateMethod is NULL, then the process will use KeDeallocateProcess as the
//   terminate method.  In that case, you must NOT use a pointer to a process which
//   wasn't allocated using KeAllocateThread.  Thus, you should ideally discard the
//   pointer as soon as this function finishes.
//
// - The process pointer is only guaranteed to be accessible (with the rigorous dis-
//   patcher database locking of course) until `TerminateMethod' is called.
void KeDetachProcess(PKPROCESS Process, PKPROCESS_TERMINATE_METHOD TerminateMethod);

// Return the current process.
PKPROCESS KeGetCurrentProcess();
