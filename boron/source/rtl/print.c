/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	rtl/print.c
	
Abstract:
	This module implements the platform independent part of the
	printing code.
	
Author:
	iProgramInCpp - 20 August 2023
***/

#define STB_SPRINTF_IMPLEMENTATION // implement the stb_sprintf right here
#include <ke.h>
#include <hal.h>
#include <string.h>

// TODO: Have a worker thread or service?
KSPIN_LOCK KiPrintLock;
KSPIN_LOCK KiDebugPrintLock;

void LogMsg(const char* msg, ...)
{
	va_list va;
	va_start(va, msg);
	char buffer[512]; // should be big enough...
	buffer[sizeof buffer - 3] = 0;
	int chars = vsnprintf(buffer, sizeof buffer - 3, msg, va);
	strcpy(buffer + chars, "\n");
	va_end(va);
	
	// this one goes to the screen
	KIPL OldIpl;
	KeAcquireSpinLock(&KiPrintLock, &OldIpl);
	HalDisplayString(buffer);
	KeReleaseSpinLock(&KiPrintLock, OldIpl);
}

#ifdef DEBUG

void DbgPrintString(const char* str);

void DbgPrint(const char* msg, ...)
{
	va_list va;
	va_start(va, msg);
	char buffer[512]; // should be big enough...
	buffer[sizeof buffer - 1] = 0;
	int chars = vsnprintf(buffer, sizeof buffer - 1, msg, va);
	strcpy(buffer + chars, "\n");
	va_end(va);
	
	// This one goes to the debug log.
#ifndef DEBUG2
	KIPL OldIpl;
	KeAcquireSpinLock(&KiDebugPrintLock, &OldIpl);
#endif
	HalPrintStringDebug(buffer);
#ifndef DEBUG2
	KeReleaseSpinLock(&KiDebugPrintLock, OldIpl);
#endif
}

#endif
