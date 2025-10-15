/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/i386/tlbs.c
	
Abstract:
	This module contains the i386 platform's specific
	TLB shootdown routine.
	
Author:
	iProgramInCpp - 14 October 2025
***/
#include <ke.h>

#ifdef CONFIG_SMP
#error If you want SMP 32-bit x86, you must copy the tlbs.c from AMD64!
#endif

#define MAX_TLBS_LENGTH 4096

void KeIssueTLBShootDown(uintptr_t Address, size_t Length)
{
	if (Length == 0)
		Length = 1;
	
	if (Length >= MAX_TLBS_LENGTH)
	{
		KeSetCurrentPageTable(KeGetCurrentPageTable());
	}
	else
	{
		for (size_t i = 0; i < Length; i++)
			KeInvalidatePage((void*)(Address + i * PAGE_SIZE));
	}
}
