/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ccbtst.c
	
Abstract:
	This module implements the CCB test for the test driver.
	It tests the functionality of the CCB (cache control block).
	
Author:
	iProgramInCpp - 26 August 2024
***/
#include <ke.h>
#include <mm.h>
#include "utils.h"

#if 0

void PerformCcbTest()
{
	LogMsg("Delaying half a second");
	PerformDelay(500, NULL);
	
	CCB Ccb;
	MmInitializeCcb(&Ccb);
	
	const int Off = 5;
	
	MmLockCcb(&Ccb);
	
	PCCB_ENTRY Entry1 = MmGetEntryPointerCcb(&Ccb, Off + 0, true);
	PCCB_ENTRY Entry2 = MmGetEntryPointerCcb(&Ccb, Off + 12, true);
	PCCB_ENTRY Entry3 = MmGetEntryPointerCcb(&Ccb, Off + 1024LL + 12, true);
	PCCB_ENTRY Entry4 = MmGetEntryPointerCcb(&Ccb, Off + 1024LL * 1024 + 1024 + 12, true);
	// L4: PCCB_ENTRY Entry5 = MmGetEntryPointerCcb(&Ccb, Off + 1024LL * 1024 * 1024 + 1024 * 1024 + 1024 + 12, true);
	// L5: PCCB_ENTRY Entry6 = MmGetEntryPointerCcb(&Ccb, Off + 1024LL * 1024 * 1024 * 1024 + 1024 * 1024 * 1024 + 1024 * 1024 + 1024 + 12, true);
	
	LogMsg("Ccb:    %p", &Ccb);
	LogMsg("Entry1: %p", Entry1);
	LogMsg("Entry2: %p", Entry2);
	LogMsg("Entry3: %p", Entry3);
	LogMsg("Entry4: %p", Entry4);
	// L4: LogMsg("Entry5: %p", Entry5);
	// L5: LogMsg("Entry6: %p", Entry6);
	
	MmUnlockCcb(&Ccb);
	
	
}

#endif
