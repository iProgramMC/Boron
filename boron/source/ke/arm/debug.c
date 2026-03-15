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

// TODO: do not hardcode the UART's location in the kernel.
//#define USE_PL011
#define USE_EXYNOS4210

void DbgPrintDouble(const char* String)
{
	DbgPrintString(String);
	HalDisplayString(String);
}

#ifdef DEBUG

#ifdef USE_PL011

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

void DbgInit()
{
	UART0_CR = 0;
	UART0_ICR = 0x7FF;
	UART0_IBRD = 13;
	UART0_FBRD = 2;
	UART0_LCRH = (3 << 5) | (1 << 4);
	UART0_CR = (1 << 0) | (1 << 8) | (1 << 9);
}

#endif // USE_PL011

#ifdef USE_EXYNOS4210

#define UART0_BASE    (0xD1800000)
#define UART0_LCON    (*(volatile unsigned int *)(UART0_BASE + 0x00))
#define UART0_CON     (*(volatile unsigned int *)(UART0_BASE + 0x04))
#define UART0_FCON    (*(volatile unsigned int *)(UART0_BASE + 0x08))
#define UART0_MCON    (*(volatile unsigned int *)(UART0_BASE + 0x0C))
#define UART0_TRSTAT  (*(volatile unsigned int *)(UART0_BASE + 0x10))
#define UART0_ERSTAT  (*(volatile unsigned int *)(UART0_BASE + 0x14))
#define UART0_FSTAT   (*(volatile unsigned int *)(UART0_BASE + 0x18))
#define UART0_MSTAT   (*(volatile unsigned int *)(UART0_BASE + 0x1C))
#define UART0_TXH     (*(volatile unsigned int *)(UART0_BASE + 0x20))
#define UART0_RXH     (*(volatile unsigned int *)(UART0_BASE + 0x24))
#define UART0_BRDIV   (*(volatile unsigned int *)(UART0_BASE + 0x28))
#define UART0_FRACVAL (*(volatile unsigned int *)(UART0_BASE + 0x2C))
#define UART0_INTP    (*(volatile unsigned int *)(UART0_BASE + 0x30))
#define UART0_INTSP   (*(volatile unsigned int *)(UART0_BASE + 0x34))
#define UART0_INTM    (*(volatile unsigned int *)(UART0_BASE + 0x38))


void DbgPrintChar(char c)
{
    while (!(UART0_TRSTAT & (1 << 1))) {
		ASM("" ::: "memory");
	}
	
    UART0_TXH = c;
}

void DbgInit()
{
	UART0_FCON = 0;
	UART0_MCON = 0;
	UART0_LCON = 0x3; // 8N1
	UART0_CON  = (1 << 2) | (1 << 0);
	UART0_BRDIV   = 53;
	UART0_FRACVAL = 4;
	DbgPrintString("ARMBoron is alive!\n");
}

#endif

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
