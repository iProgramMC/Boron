/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	cc/cci.h
	
Abstract:
	This header defines cache manager internal
	function prototypes.
	
Author:
	iProgramInCpp - 4 June 2025
***/
#pragma once

#include <cc.h>
#include <mm.h>

static ALWAYS_INLINE inline
void CcAcquireMutex(PKMUTEX Mutex)
{
	UNUSED BSTATUS Status;
	
	Status = KeWaitForSingleObject(Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	ASSERT(SUCCEEDED(Status));
}
