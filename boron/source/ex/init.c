/***
	The Boron Operating System
	Copyright (C) 2023-2024 iProgramInCpp

Module name:
	ex/init.c
	
Abstract:
	This module implements the initialization of the executive.
	The executive is defined as the non-core part of the kernel.
	
Author:
	iProgramInCpp - 18 December 2023
***/
#include "exp.h"

bool ExInitSystem()
{
	if (!ExpCreateMutexType())
		return false;
	
	if (!ExpCreateEventType())
		return false;
	
#if 0
	if (!ExpCreateSemaphoreType())
		return false;
	
	if (!ExpCreateTimerType())
		return false;
	
	if (!ExpCreateThreadType())
		return false;
	
	if (!ExpCreateProcessType())
		return false;
#endif
	
	// TODO:
	// After creating the process type, create the System process?
	// Three ways to do this:
	// 1. Shift all references from PsInitProcess to the new object.
	// 2. Make the System process a shadow of PsSystemProcess or
	// 3. Don't create a process object for the system at all.
	return true;
}

// This routine initializes the executive layer, that is, the part
// of the kernel that's implemented on top of the kernel core.
NO_RETURN void ExpInitializeExecutive(UNUSED void* Context)
{
	if (!ObInitSystem())
		KeCrash("Could not initialize object manager");
	
	if (!ExInitSystem())
		KeCrash("Could not initialize executive");
	
	if (!IoInitSystem())
		KeCrash("Could not initialize I/O manager");
	
	KeTerminateThread(0);
}

