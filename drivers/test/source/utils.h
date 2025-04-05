/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	utils.h
	
Abstract:
	This header file provides forward declarations for utilitary
	functions in the test driver module.
	
Author:
	iProgramInCpp - 19 November 2023
***/
#pragma once

#include <ke.h>
#include <ob.h>

// Create a simple thread.
//
// Use ObDereferenceObject on the thread pointer to detach and/or free!
PKTHREAD CreateThread(PKTHREAD_START StartRoutine, void* Parameter);

// Performs a small delay.
void PerformDelay(int Ms, PKDPC Dpc);

// Performs a hex dump of some memory.
void DumpHex(void* DataV, size_t DataSize, bool LogScreen);
