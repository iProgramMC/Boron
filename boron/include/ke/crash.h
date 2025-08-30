/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/crash.h
	
Abstract:
	This header file contains the definitions related
	to the crash functions.
	
Author:
	iProgramInCpp - 28 October 2023
***/
#pragma once

#include <main.h>

NO_RETURN void KeCrash(const char* message, ...);
NO_RETURN void KeCrashBeforeSMPInit(const char* message, ...);

#if defined(IS_HAL) || defined(KERNEL)
NO_RETURN void KeCrashConclusion(const char* Message);
#endif
