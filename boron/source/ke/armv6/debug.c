/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/debug.c
	
Abstract:
	This module implements architecture specific debugging
	routines.
	
Author:
	iProgramInCpp - 26 December 2025
***/
#include <ke.h>

void DbgPrintDouble(const char* String)
{
	DbgPrintString(String);
	HalDisplayString(String);
}

#ifdef DEBUG

#define UART0_BASE (0xD18F1000)
#define UART0_DR   (*(volatile unsigned int *)(UART0_BASE + 0x00))
#define UART0_FR   (*(volatile unsigned int *)(UART0_BASE + 0x18))

void DbgPrintChar(char c)
{
    while (UART0_FR & (1 << 5)) {
		ASM("" ::: "memory");
	}
	
    UART0_DR = c;
}

void DbgPrintString(const char* str)
{
	while (*str)
	{
		if (*str == '\n')
			DbgPrintChar('\r');
		DbgPrintChar(*str);
		str++;
	}
}

void DbgInit()
{
	// TODO
}

void DbgPrintStackTrace(uintptr_t Rbp)
{
	(void) Rbp;
	
	DbgPrintDouble("TODO: DbgPrintStackTrace\n");
}

KSPIN_LOCK KiPrintLock;
KSPIN_LOCK KiDebugPrintLock;

void DbgPrintStringLocked(const char* str)
{
#ifndef DONT_LOCK
	KIPL OldIpl;
	KeAcquireSpinLock(&KiDebugPrintLock, &OldIpl);
#endif

	DbgPrintString(str);
	
#ifndef DONT_LOCK
	KeReleaseSpinLock(&KiDebugPrintLock, OldIpl);
#endif
}

#endif
