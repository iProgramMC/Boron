/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/thread.c
	
Abstract:
	This module implements the initialization of the executive.
	The executive is defined as the non-core part of the kernel.
	
Author:
	iProgramInCpp - 18 December 2023
***/
#include <ke.h>
#include <ob.h>
#include <io.h>

// This routine initializes the executive layer, that is, the part
// of the kernel that's implemented on top of the kernel core.
NO_RETURN void ExpInitializeExecutive(UNUSED void* Context)
{
	if (!ObInitSystem())
		KeCrash("Could not initialize object manager");
	
	if (!IoInitSystem())
		KeCrash("Could not initialize I/O manager");
	
	KeTerminateThread(0);
}

