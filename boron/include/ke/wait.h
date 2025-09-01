/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/wait.h
	
Abstract:
	This header file contains the definitions for the
	available wait operations.
	
Author:
	iProgramInCpp - 11 November 2023
***/
#pragma once

#include <ke/mode.h>

// Wait for multiple objects at once.
// Use a value of zero to poll the objects.
// Use a value of TIMEOUT_INFINITE to specify that timeout isn't needed.
BSTATUS KeWaitForMultipleObjects(
	int Count,
	void* Objects[],
	int WaitType,
	bool Alertable,
	int TimeoutMS,
	PKWAIT_BLOCK WaitBlockArray,
	KPROCESSOR_MODE WaitMode);

// Wait for a single object.
BSTATUS KeWaitForSingleObject(void* Object, bool Alertable, int TimeoutMS, KPROCESSOR_MODE WaitMode);
