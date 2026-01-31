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
#define UART0_DR    (*(volatile unsigned int *)(UART0_BASE + 0x00))
#define UART0_FR    (*(volatile unsigned int *)(UART0_BASE + 0x18))
#define UART0_IBRD  (*(volatile unsigned int *)(UART0_BASE + 0x24))
#define UART0_FBRD  (*(volatile unsigned int *)(UART0_BASE + 0x28))
#define UART0_LCRH  (*(volatile unsigned int *)(UART0_BASE + 0x2C))
#define UART0_CR    (*(volatile unsigned int *)(UART0_BASE + 0x30))
#define UART0_ICR   (*(volatile unsigned int *)(UART0_BASE + 0x44))

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
	UART0_CR = 0;
	UART0_ICR = 0x7FF;
	UART0_IBRD = 13;
	UART0_FBRD = 2;
	UART0_LCRH = (3 << 5) | (1 << 4);
	UART0_CR = (1 << 0) | (1 << 8) | (1 << 9);
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
