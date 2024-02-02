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

typedef struct EPROCESS_tag EPROCESS, *PEPROCESS;

struct EPROCESS_tag
{
	// The kernel side process.
	KPROCESS Pcb;
	
	// Rwlock that guards the address space of the process.
	EX_RW_LOCK AddressLock;
	
	// Handle table.
	void* HandleTable;
};

#define PsGetCurrentProcess() ((PEPROCESS)KeGetCurrentProcess())
