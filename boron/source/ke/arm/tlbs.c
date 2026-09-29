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

void KeIssueTLBShootDown(uintptr_t Address, size_t LengthPages, UNUSED PKPROCESS Process)
{
	if (LengthPages == 0)
		LengthPages = 1;
	
	if (LengthPages >= MAX_TLBS_LENGTH)
	{
		KeFlushTLB();
	}
	else
	{
		for (size_t i = 0; i < LengthPages; i++)
			KeInvalidatePage((void*)(Address + i * PAGE_SIZE));
	}
}
