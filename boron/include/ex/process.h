/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/process.h
	
Abstract:
	This header file defines the executive process structure.
	
Author:
	iProgramInCpp - 8 January 2024
***/
#pragma once

#include <ke/process.h>
#include <mm/vad.h>
#include <ex/rwlock.h>

typedef struct EPROCESS_tag EPROCESS, *PEPROCESS;

struct EPROCESS_tag
{
	// The kernel side process.
	KPROCESS Pcb;
	
	// The Virtual Address Descriptor list of this process.
	MMVAD_LIST VadList;
	
	// The heap manager for this process.
	MMHEAP Heap;
	
	// Rwlock that guards the address space of the process.
	// TODO:  Perhaps this should be replaced by the VAD list lock?
	EX_RW_LOCK AddressLock;
	
	// Object handle table.  This handle table manages objects opened by the process.
	void* HandleTable;
};

#define PsGetCurrentProcess() ((PEPROCESS)KeGetCurrentProcess())

// Attaches to or detaches from a process, on behalf of the current thread.
//
// If NewProcess is NULL, then the current thread is detached from the
// processes and the active address space will be the address space of
// the host process.
PEPROCESS PsSetAttachedProcess(PEPROCESS NewProcess);

PEPROCESS PsGetAttachedProcess();
