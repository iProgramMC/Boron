/***
	The Boron Operating System
	Copyright (C) 2023-2025 iProgramInCpp

Module name:
	ex/init.c
	
Abstract:
	This module implements the initialization of the executive.
	The executive is defined as the non-core part of the kernel.
	
Author:
	iProgramInCpp - 18 December 2023
***/
#include "exp.h"
#include <io.h>
#include <ps.h>
#include <ldr.h>
#include <tty.h>

INIT
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
#endif
	
	ExInitBootConfig();
	return true;
}

// This routine initializes the executive layer, that is, the part
// of the kernel that's implemented on top of the kernel core.
INIT
NO_RETURN void ExpInitializeExecutive(UNUSED void* Context)
{
	if (!ObInitSystem())
		KeCrash("Could not initialize object manager");
	
	if (!ExInitSystem())
		KeCrash("Could not initialize executive");
	
	if (!MmInitSystem())
		KeCrash("Could not initialize memory manager");
	
	if (!PsInitSystem())
		KeCrash("Could not initialize process manager");
	
	if (!IoInitSystem())
		KeCrash("Could not initialize I/O manager");
	
	if (!LdrPrepareInitialRoot())
		KeCrash("Could not prepare initial root");
	
	if (!PsInitSystemPart2())
		KeCrash("Could not initialize process manager - part 2");
	
	if (!TtyInitSystem())
		KeCrash("Could not initialize pseudoterminal subsystem");
	
	if (!ObLinkRootDirectory())
		KeCrash("Could not create a symbolic link to the root directory");
	
	// TODO: Crash inside of these functions instead of returning false.
	// It'll be more useful because those functions actually have the
	// context needed to resolve the bug.
	//
	// We should do it like this:
	MmInitializeModifiedPageWriter();
	
	KeTerminateThread(0);
}

