/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/process.h
	
Abstract:
	This header file defines the kernel thread object
	and its manipulation functions.
	
Author:
	iProgramInCpp - 22 November 2023
***/
#pragma once

#include <ke/dispatch.h>
#include <mm.h>

typedef struct KPROCESS_tag KPROCESS, *PKPROCESS;

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

// Detach the child process from the current process' execution.
// * Must only be performed on processes allocated with KeAllocateProcess.
//
// * It will be cleaned up by the system once it terminates,
//   instead of having to wait on it to clean it up.
//
// * After the call, treat the passed pointer as invalid and throw it away ASAP.
void KeDetachProcess(PKPROCESS Process);

// Return the current process.
PKPROCESS KeGetCurrentProcess();
