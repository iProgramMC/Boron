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

PMMVAD_LIST MmLockVadListProcess(PEPROCESS Process)
{
	UNUSED BSTATUS Status = KeWaitForSingleObject(&Process->VadList.Mutex, false, TIMEOUT_INFINITE);
	ASSERT(Status == STATUS_SUCCESS);
	
	return &Process->VadList;
}

void MmUnlockVadList(PMMVAD_LIST VadList)
{
	KeReleaseMutex(&VadList->Mutex);
}

void MmInitializeVadList(PMMVAD_LIST VadList)
{
	KeInitializeMutex(&VadList->Mutex, MM_VAD_MUTEX_LEVEL);
	InitializeRbTree(&VadList->Tree);
}