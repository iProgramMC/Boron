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

#define CURRENT_PROCESS_HANDLE ((HANDLE) 0xFFFFFFFF)
#define CURRENT_THREAD_HANDLE  ((HANDLE) 0xFFFFFFFE)

typedef struct EPROCESS_tag EPROCESS, *PEPROCESS;

struct EPROCESS_tag
{
	// The kernel side process.
	KPROCESS Pcb;
	
	// The Virtual Address Descriptor of this process.
	MMVAD Vad;
	
	// Rwlock that guards the address space of the process.
	// TODO:  Perhaps this should be replaced by the VAD list lock?
	EX_RW_LOCK AddressLock;
	
	// Object handle table.  This handle table manages objects opened by the process.
	void* HandleTable;
};

#define PsGetCurrentProcess() ((PEPROCESS)KeGetCurrentProcess())
