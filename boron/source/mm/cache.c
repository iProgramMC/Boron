/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/cache.c
	
Abstract:
	This module implements the cache control block (CCB)
	object for the memory manager.
	
Author:
	iProgramInCpp - 18 August 2024
***/
#include "mi.h"

void MmInitializeCcb(PCCB Ccb)
{
	memset(Ccb, 0, sizeof *Ccb);
	
	KeInitializeMutex(&Ccb->Mutex, MM_CCB_MUTEX_LEVEL);
}


