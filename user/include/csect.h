/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	csect.h
	
Abstract:
	This header defines heap APIs for Boron userspace applications.
	
Author:
	iProgramInCpp - 14 July 2025
***/
#pragma once

#include <handle.h>

typedef struct
{
	// Is it locked?
	int Locked;

	// Amount of times to spin on Locked before deferring to a system call
	int MaxSpins;

	HANDLE EventHandle;
}
OS_CRITICAL_SECTION, *POS_CRITICAL_SECTION;
