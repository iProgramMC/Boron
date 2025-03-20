/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	queue.c
	
Abstract:
	This module implements the queue management functions
	for Boron's NVMe disk driver.
	
Author:
	iProgramInCpp - 25 July 2024
***/
#include "nvme.h"

static void NvmeDpc(PKDPC Dpc, void* Context, UNUSED void* SystemArgument1, UNUSED void* SystemArgument2)
{
	PQUEUE_CONTROL_BLOCK Qcb = Context;
	ASSERT(CONTAINING_RECORD(Dpc, QUEUE_CONTROL_BLOCK, Dpc) == Qcb);
	
	KIPL Ipl;
	KeAcquireSpinLock(&Qcb->SpinLock, &Ipl);
	
	PNVME_COMPLETION_QUEUE_ENTRY CompletionQueue = Qcb->CompletionQueue.Address;
	int Count = 0;
	
	while (CompletionQueue[Qcb->CompletionQueue.Index].Status.Phase == Qcb->CompletionQueue.Phase)
	{
		int SubmissionId = CompletionQueue[Qcb->CompletionQueue.Index].CommandIdentifier;
		
		// Copy the completion queue entry.
		Qcb->Entries[SubmissionId]->Comp = CompletionQueue[Qcb->CompletionQueue.Index];
		
		// Set the event.
		KeSetEvent(Qcb->Entries[SubmissionId]->Event, NVME_PRIORITY_BOOST);
		
		// Clear the entry in the Entries list and release the entry semaphore.
		ASSERT(SubmissionId >= 0 && SubmissionId < (int)ARRAY_COUNT(Qcb->Entries) && Qcb->Entries[SubmissionId] != NULL);
		
		Qcb->Entries[SubmissionId] = NULL;
		KeReleaseSemaphore(&Qcb->Semaphore, 1, NVME_PRIORITY_BOOST);
		
		// Increment the pair's completion queue index in a round fashion.
		Qcb->CompletionQueue.Index = (Qcb->CompletionQueue.Index + 1) % Qcb->CompletionQueue.EntryCount;
		if (Qcb->CompletionQueue.Index == 0)
			Qcb->CompletionQueue.Phase ^= 1;
		
		Count++;
	}
	
	if (Count)
		*Qcb->CompletionQueue.DoorBell = Qcb->CompletionQueue.Index;
	
	KeReleaseSpinLock(&Qcb->SpinLock, Ipl);
}

static void NvmeService(PKINTERRUPT Interrupt, void* Context)
{
	PQUEUE_CONTROL_BLOCK Qcb = Context;
	ASSERT(Qcb == CONTAINING_RECORD(Interrupt, QUEUE_CONTROL_BLOCK, Interrupt));
	
	KeEnqueueDpc(&Qcb->Dpc, NULL, NULL);
}

static void NvmeCreateInterruptForQueue(PQUEUE_CONTROL_BLOCK Qcb, int MsixIndex)
{
	PPCI_DEVICE Device = Qcb->Controller->PciDevice;
	KIPL Ipl;
	int Vector = AllocateVector(&Ipl, IPL_DEVICES1);
	if (Vector < 0)
		ASSERT(Vector >= 0);
	
	KeInitializeDpc(&Qcb->Dpc, NvmeDpc, Qcb);
	
	KeInitializeInterrupt(
		&Qcb->Interrupt,
		NvmeService,
		Qcb,
		&Qcb->InterruptSpinLock,
		Vector,
		Ipl,
		false
	);
	
	UNUSED bool Connected = KeConnectInterrupt(&Qcb->Interrupt);
	ASSERT(Connected);
	
	HalPciMsixSetInterrupt(Device, MsixIndex, KeGetCurrentPRCB()->LapicId, Vector, false, true);
	
	HalPciMsixSetFunctionMask(Device, false);
}

// Send a command to a queue and wait for it to finish.
//
// NOTE: The PKEVENT Event field of EntryPair must point to a valid event.
void NvmeSend(PQUEUE_CONTROL_BLOCK Qcb, PQUEUE_ENTRY_PAIR EntryPair)
{
	// Wait on the semaphore to ensure there's space for our request.
	//
	// The semaphore will get released by the DPC routine when an operation
	// has completed.
	KeWaitForSingleObject(&Qcb->Semaphore, false, TIMEOUT_INFINITE, MODE_KERNEL);
	
	KIPL Ipl;
	KeAcquireSpinLock(&Qcb->SpinLock, &Ipl);
	
	PNVME_SUBMISSION_QUEUE_ENTRY SubmissionQueue = Qcb->SubmissionQueue.Address;
	
	// Find a free spot to place this pointer in the Qcb's Entries list.
	int Index = 0;
	while (Qcb->Entries[Index]) Index++;
	
	// This shouldn't happen as the Qcb's Semaphore stops us from reaching the limit.
	ASSERT(Index != (int)ARRAY_COUNT(Qcb->Entries));
	
	EntryPair->Sub.CommandHeader.CommandIdentifier = Index;
	
	Qcb->Entries[Index] = EntryPair;
	
	// Add and advance.
	SubmissionQueue[Qcb->SubmissionQueue.Index] = EntryPair->Sub;
	Qcb->SubmissionQueue.Index = (Qcb->SubmissionQueue.Index + 1) % Qcb->SubmissionQueue.EntryCount;
	
	// Set the door bell.
	*Qcb->SubmissionQueue.DoorBell = Qcb->SubmissionQueue.Index;
	
	KeReleaseSpinLock(&Qcb->SpinLock, Ipl);
}

void NvmeSetupQueue(
	PCONTROLLER_EXTENSION ContExtension,
	PQUEUE_CONTROL_BLOCK Qcb,
	uintptr_t SubmissionQueuePhysical,
	uintptr_t CompletionQueuePhysical,
	int DoorBellIndex,
	int MsixIndex,
	size_t SubmissionQueueCount,
	size_t CompletionQueueCount
)
{
	Qcb->Controller = ContExtension;
	
	ASSERT(SubmissionQueueCount);
	ASSERT(CompletionQueueCount);
	
	Qcb->SubmissionQueuePhysical = SubmissionQueuePhysical;
	Qcb->CompletionQueuePhysical = CompletionQueuePhysical;
	
	PQUEUE_ACCESS_BLOCK Queue = &Qcb->SubmissionQueue;
	Queue->Address    = MmGetHHDMOffsetAddr(SubmissionQueuePhysical);
	Queue->EntryCount = SubmissionQueueCount;
	Queue->DoorBell   = GetDoorBell(ContExtension->Controller->DoorBells, DoorBellIndex, false, ContExtension->DoorbellStride);
	Queue->Phase      = 0;
	Queue->Index      = 0;
	
	Queue = &Qcb->CompletionQueue;
	Queue->Address    = MmGetHHDMOffsetAddr(CompletionQueuePhysical);
	Queue->EntryCount = CompletionQueueCount;
	Queue->DoorBell   = GetDoorBell(ContExtension->Controller->DoorBells, DoorBellIndex, true, ContExtension->DoorbellStride);
	Queue->Phase      = 1;
	Queue->Index      = 0;
	
	KeInitializeDpc(&Qcb->Dpc, NvmeDpc, &ContExtension->AdminQueue);
	KeInitializeSpinLock(&Qcb->SpinLock);
	KeInitializeSemaphore(&Qcb->Semaphore, ARRAY_COUNT(Qcb->Entries), SEMAPHORE_LIMIT_NONE);
	
	// Create the queue interrupt.
	NvmeCreateInterruptForQueue(Qcb, MsixIndex);
}

PQUEUE_CONTROL_BLOCK NvmeChooseIoQueue(PCONTROLLER_EXTENSION Extension)
{
	// NOTE: Unbounded increase, but this is fine.  If it overflows,
	// even if the I/O queue count is not a power of 2, it'll still
	// work. Just that some of the IO queues will be rarely used more
	// often than the others.
	size_t Index = AtFetchAdd(Extension->ActiveQueueIndex, 1);
	Index %= Extension->IoQueueCount;
	return &Extension->IoQueues[Index];
}
