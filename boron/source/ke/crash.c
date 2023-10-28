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
#include <ldr.h>
#include <string.h>

extern bool KiSmpInitted;

void KeCrash(const char* message, ...)
{
	if (!KiSmpInitted) {
		KeCrashBeforeSMPInit("called KeCrash before SMP init?! (message: %s, RA: %p)", message, __builtin_return_address(0));
	}
	
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

extern KSPIN_LOCK KiPrintLock;
extern KSPIN_LOCK KiDebugPrintLock;

void KeCrashConclusion(const char* Message)
{
	static char CrashBuffer[4096];
	
	// NOTE: We are running in a single processor context - all other processors were shut down
	KiPrintLock.Locked = 0;
	KiDebugPrintLock.Locked = 0;
	
	snprintf(CrashBuffer, sizeof CrashBuffer, "\n\x1B[91m*** STOP (CPU %u): \x1B[0m %s\n", KeGetCurrentPRCB()->LapicId, Message);
	
	HalDisplayString(CrashBuffer);
	DbgPrintString(CrashBuffer);
	
	// List each loaded DLL's base
	LogMsg("Dll Base         Name");
	for (int i = 0; i < KeLoadedDLLCount; i++)
	{
		PLOADED_DLL LoadedDll = &KeLoadedDLLs[i];
		
		LogMsg("%p %s", LoadedDll->ImageBase, LoadedDll->Name);
	}
	
	LogMsg("\n");
	
	DbgPrintStackTrace(0);
	
	// Now that all that's done, HALT!
	KeStopCurrentCPU();
}
