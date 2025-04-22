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

#include <main.h>
#include <string.h>

#ifdef KERNEL
#include <ke.h>
#include <hal.h>
#else
#include <boron.h>
#endif

#if defined(KERNEL) && defined(DEBUG)
// TODO: Have a worker thread or service?
extern KSPIN_LOCK KiPrintLock;
extern KSPIN_LOCK KiDebugPrintLock;
#endif

void LogMsg(const char* msg, ...)
{
	va_list va;
	va_start(va, msg);
	char buffer[512]; // should be big enough...
	buffer[sizeof buffer - 3] = 0;
	int chars = vsnprintf(buffer, sizeof buffer - 3, msg, va);
	strcpy(buffer + chars, "\n");
	va_end(va);
	
#ifdef KERNEL
	HalDisplayString(buffer);
#else
	OSOutputDebugString(buffer, strlen(buffer));
#endif
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
	
#ifdef KERNEL
	// This one goes to the debug log.
	// Debug2 turns off the spin locks associated with the debug prints.
#ifndef DEBUG2
	KIPL OldIpl;
	KeAcquireSpinLock(&KiDebugPrintLock, &OldIpl);
#endif

	HalPrintStringDebug(buffer);
#ifndef DEBUG2
	KeReleaseSpinLock(&KiDebugPrintLock, OldIpl);
#endif

#else // KERNEL
	// Libboron.so has a special way of printing messages.
	OSOutputDebugString(buffer, strlen(buffer));
#endif
}

#endif
