/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/arm/debug.c
	
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

void DbgPrintStackTrace(uintptr_t Rbp)
{
	(void) Rbp;
	
	DbgPrintDouble("TODO: DbgPrintStackTrace\n");
}

void DbgDumpPageTables()
{
	uintptr_t Ttbr0 = (uintptr_t) KeGetCurrentPageTable();
	
	KeFlushTLB();
	KeSweepIcache();
	KeSweepDcache();
	
	// we're lucky that (right now) the entire system memory fits inside 256MB
	// otherwise we'd be screwed
	
	DbgPrint("Dumping page table at %p:", Ttbr0);
	
	uintptr_t* L1 = (uintptr_t*) (0xC0000000 | Ttbr0);
	for (int i = 0; i < 4096; i++)
	{
		uintptr_t L1Pte = L1[i];
		uintptr_t PhysPage;
		int L1Type = L1Pte & 0b11;
		if (L1Type == 0b00)
			// Unmapped
			continue;
		
		if (L1Type == 0b10)
		{
			// Section
			int Ap = (L1Pte >> 10) & 0b11;
			int Tex = (L1Pte >> 12) & 0b111;
			int Cb = (L1Pte >> 2) & 0b11;
			
			int SuperSection = L1Pte & (1 << 18);
			PhysPage = L1Pte & ~((1 << 20) - 1);
			
			DbgPrint("	%p - %d - %p - %s AP:%x TEX:%x CB:%x  RawPte:%p",
				i << 20,
				i,
				PhysPage,
				SuperSection ? "super-section" : "section",
				Ap,
				Tex,
				Cb,
				L1Pte
			);
			
			continue;
		}
		else if (L1Type == 0b11)
		{
			DbgPrint("	%p - %d - Reserved (this entry's presence is an error) RawPte:%p", i << 20, i, L1Pte);
			continue;
		}
		
		// Coarse Page Table
		PhysPage = L1Pte & ~((1 << 10) - 1);
		DbgPrint("	%p - %d - %p - Coarse Page Table  RawPte:%p", i << 20, i, PhysPage, L1Pte);
		
		uintptr_t* L2 = (uintptr_t*) (0xC0000000 | PhysPage);
		
		for (int j = 0; j < 256; j++)
		{
			uintptr_t Pte = L2[j];
			uintptr_t RealAddr = (i << 20) | (j << 12);
			int Type = Pte & 0b11;
			
			if (Type == 0b00)
				// Unmapped
				continue;
			
			PhysPage = Pte & ~((1 << 12) - 1);
			if (Type == 0b01)
			{
				DbgPrint(
					"		%p - %d - %d - %p - Large Page (this entry's presence is an error) RawPte:%p",
					RealAddr,
					i,
					j,
					PhysPage,
					Pte
				);
				continue;
			}
			
			bool IsNX = Type == 0b11;
			int Ap = (Pte >> 4) & 0b11;
			int Tex = (Pte >> 6) & 0b111;
			int Cb = (Pte >> 2) & 0b11;
			int Apx = (Pte >> 9) & 0b1;
			int S = (Pte >> 10) & 0b1;
			int nG = (Pte >> 11) & 0b1;
			
			DbgPrint(
				"		%p - %d - %d - %p - page NX:%d TEX:%x CB:%x AP:%x APX:%x S:%d NG:%d  RawPte:%p",
				RealAddr,
				i,
				j,
				PhysPage,
				IsNX,
				Tex,
				Cb,
				Ap,
				Apx,
				S,
				nG,
				Pte
			);
		}
	}
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

#elif defined USE_EXYNOS4210

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

#else

// no debug for you, scream into the void
void DbgPrintChar(char c)
{
	(void) c;
}

void DbgInit()
{
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
