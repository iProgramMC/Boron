/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/vad.c
	
Abstract:
	
	
Author:
	iProgramInCpp - 18 August 2024
***/
#include <mm.h>
#include <ex.h>

PMMVAD MmLockVadListProcess(PEPROCESS Process)
{
	UNUSED BSTATUS Status = KeWaitForSingleObject(&Process->Vad.Mutex, false, TIMEOUT_INFINITE);
	ASSERT(Status == STATUS_SUCCESS);
	
	return &Process->Vad;
}

void MmUnlockVadList(PMMVAD Vad)
{
	KeReleaseMutex(&Vad->Mutex);
}
