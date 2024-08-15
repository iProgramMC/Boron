/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/init.c
	
Abstract:
	This module implements the initialization code for the
	process manager in Boron.
	
Author:
	iProgramInCpp - 26 November 2023
***/
#include <ps.h>

static EPROCESS PsSystemProcess;

PEPROCESS PsGetSystemProcess()
{
	return &PsSystemProcess;
}

INIT
void PsInitSystemProcess()
{
	KeInitializeProcess(
		&PsSystemProcess.Pcb,
		PRIORITY_NORMAL,
		AFFINITY_ALL
	);
	
	// Initialize the address lock.
	ExInitializeRwLock(&PsSystemProcess.AddressLock);
	
	// Initialize the handle table.
	// TODO: Defaults chosen arbitrarily.
	if (FAILED(ExCreateHandleTable(16, 16, 2048, 1, &PsSystemProcess.HandleTable)))
	{
		KeCrash("Ps: Could not create handle table for System process");
	}
}
