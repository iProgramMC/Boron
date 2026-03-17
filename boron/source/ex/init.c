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
	LogMsg("Calling ExpCreateMutexType");
	if (!ExpCreateMutexType())
		return false;
	
	LogMsg("Calling ExpCreateEventType");
	if (!ExpCreateEventType())
		return false;
	
#if 0
	if (!ExpCreateSemaphoreType())
		return false;
	
	if (!ExpCreateTimerType())
		return false;
#endif
	
	LogMsg("Calling ExInitBootConfig");
	ExInitBootConfig();
	return true;
}

// This routine initializes the executive layer, that is, the part
// of the kernel that's implemented on top of the kernel core.
INIT
NO_RETURN void ExpInitializeExecutive(UNUSED void* Context)
{
	LogMsg("Calling ObInitSystem");
	if (!ObInitSystem())
		KeCrash("Could not initialize object manager");
	
	LogMsg("Calling ExInitSystem");
	if (!ExInitSystem())
		KeCrash("Could not initialize executive");
	
	LogMsg("Calling MmInitSystem");
	if (!MmInitSystem())
		KeCrash("Could not initialize memory manager");
	
	LogMsg("Calling PsInitSystem");
	if (!PsInitSystem())
		KeCrash("Could not initialize process manager");
	
	LogMsg("Calling IoInitSystem");
	if (!IoInitSystem())
		KeCrash("Could not initialize I/O manager");
	
	LogMsg("Calling LdrPrepareInitialRoot");
	if (!LdrPrepareInitialRoot())
		KeCrash("Could not prepare initial root");
	
	LogMsg("Calling PsInitSystemPart2");
	if (!PsInitSystemPart2())
		KeCrash("Could not initialize process manager - part 2");
	
	LogMsg("Calling TtyInitSystem");
	if (!TtyInitSystem())
		KeCrash("Could not initialize pseudoterminal subsystem");
	
	LogMsg("Calling ObLinkRootDirectory");
	if (!ObLinkRootDirectory())
		KeCrash("Could not create a symbolic link to the root directory");
	
	// TODO: Crash inside of these functions instead of returning false.
	// It'll be more useful because those functions actually have the
	// context needed to resolve the bug.
	//
	// We should do it like this:
	LogMsg("Calling MmInitializeModifiedPageWriter");
	MmInitializeModifiedPageWriter();
	
	LogMsg("Executive Initialization done");
	KeTerminateThread(0);
}

