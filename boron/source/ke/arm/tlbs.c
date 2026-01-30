/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/tlbs.c
	
Abstract:
	This module contains the armv6 platform's specific
	TLB shootdown routine.
	
Author:
	iProgramInCpp - 29 December 2025
***/
#include <ke.h>

#ifdef CONFIG_SMP
#error 32-bit ARM SMP is not supported!
#endif

#define MAX_TLBS_LENGTH 4096

void KeIssueTLBShootDown(uintptr_t Address, size_t Length)
{
	if (Length == 0)
		Length = 1;
	
	if (Length >= MAX_TLBS_LENGTH)
	{
		KeFlushTLB();
	}
	else
	{
		for (size_t i = 0; i < Length; i++)
			KeInvalidatePage((void*)(Address + i * PAGE_SIZE));
	}
}
