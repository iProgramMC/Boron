/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/crash.c
	
Abstract:
	This module contains the crash handler thunk.
	Its job is to format the message and invoke the HAL
	to perform the crash itself.
	
Author:
	iProgramInCpp - 16 September 2023
***/

#include <ke.h>
#include <hal.h>
#include <string.h>

void KeCrash(const char* message, ...)
{
	// format the message
	va_list va;
	va_start(va, message);
	char buffer[1024]; // may be a beefier message than a garden variety LogMsg
	buffer[sizeof buffer - 3] = 0;
	int chars = vsnprintf(buffer, sizeof buffer - 3, message, va);
	strcpy(buffer + chars, "\n");
	va_end(va);
	
	HalCrashSystem(buffer);
}
