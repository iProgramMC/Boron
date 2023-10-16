/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/thread.c
	
Abstract:
	This header file contains the implementation of the
	thread object.
	
Author:
	iProgramInCpp - 2 October 2023
***/
#include "ki.h"

PKTHREAD KeCreateEmptyThread()
{
	PKTHREAD Thread = ExAllocateSmall(sizeof(KTHREAD));
	if (!Thread)
		return NULL;
	
	memset(Thread, 0, sizeof *Thread);
	
	Thread->Status = KTHREAD_STATUS_UNINITIALIZED;
	
	return Thread;
}

// TODO: Process Parameter at the end (PKPROCESS Process)
void KeInitializeThread(PKTHREAD Thread, EXMEMORY_HANDLE KernelStack, PKTHREAD_START StartRoutine, void* StartContext)
{
	if (!KernelStack)
		KernelStack = ExAllocatePool(POOL_FLAG_NON_PAGED, KERNEL_STACK_SIZE / PAGE_SIZE, NULL, EX_TAG("ThSt"));
	
	Thread->StartRoutine = StartRoutine;
	Thread->StartContext = StartContext;
	
	Thread->Stack.Top    = ExGetAddressFromHandle(KernelStack);
	Thread->Stack.Size   = ExGetSizeFromHandle(KernelStack) * PAGE_SIZE;
	Thread->Stack.Handle = KernelStack;
	
	KiSetupRegistersThread(Thread);
	
	// Despite that the entry list will be part of the scheduler's thread list, it currently isn't part
	// of the scheduler. Initialize it as an empty list head so we can check it in KeSetPriorityThread.
	InitializeListHead(&Thread->EntryQueue);
	
	Thread->Status = KTHREAD_STATUS_INITIALIZED;
}
