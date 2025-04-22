/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	exs.h
	
Abstract:
	This header file contains the publicly exposed structure
	definitions for Boron's Executive Subsystem.
	
Author:
	iProgramInCpp - 22 April 2025
***/
#pragma once

#include "handle.h"

enum
{
	WAIT_ALL_OBJECTS,
	WAIT_ANY_OBJECT,
	__WAIT_TYPE_COUNT,
};

#define WAIT_TIMEOUT_INFINITE (0x7FFFFFFF)

#define CURRENT_PROCESS_HANDLE ((HANDLE) 0xFFFFFFFF)
#define CURRENT_THREAD_HANDLE  ((HANDLE) 0xFFFFFFFE)
