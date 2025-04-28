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
};

// Allocate an uninitialized process instance.  Use when you want to detach the process.
PKPROCESS KeAllocateProcess();

// Get the system process.
PKPROCESS KeGetSystemProcess();

// Deallocate a process allocated with KeAllocateProcess.
void KeDeallocateProcess(PKPROCESS Process);

// Initialize the process.
void KeInitializeProcess(PKPROCESS Process, int BasePriority, KAFFINITY BaseAffinity);

// Attach to or detach from a process, on behalf of the current thread.
//
// If Process is NULL, the thread is detached from any process
// it was attached to, and the active address space will be the
// address space of the host process.
PKPROCESS KeSetAttachedProcess(PKPROCESS Process);

// Get a pointer to the current process.
PKPROCESS KeGetCurrentProcess();
