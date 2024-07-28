/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	commands.c
	
Abstract:
	This module implements functions to send commands to the
	NVMe disk controller.
	
Author:
	iProgramInCpp - 25 July 2024
***/
#include "nvme.h"
#include <string.h>

// NOTE: Here, EntryPair's Event field will be replaced with an address that'll be stale on exit.
BSTATUS NvmeSendAndWait(PQUEUE_CONTROL_BLOCK Qcb, PQUEUE_ENTRY_PAIR EntryPair)
{
	KEVENT Event;
	KeInitializeEvent(&Event, EVENT_NOTIFICATION, false);
	EntryPair->Event = &Event;
	
	NvmeSend(Qcb, EntryPair);
	
	return KeWaitForSingleObject(&Event, false, TIMEOUT_INFINITE);
}

BSTATUS NvmeIdentify(PCONTROLLER_EXTENSION ContExtension, void* IdentifyBuffer, uint32_t Cns, uint32_t NamespaceId)
{
	QUEUE_ENTRY_PAIR QueueEntry;
	memset(&QueueEntry, 0, sizeof QueueEntry);
	
	// note: PRP = 0, FusedOperation = 0
	QueueEntry.Sub.CommandHeader.OpCode = ADMOP_IDENTIFY;
	QueueEntry.Sub.NamespaceId = NamespaceId;
	
	// Allocate a memory page to receive identification information.
	int Page = MmAllocatePhysicalPage();
	if (Page == PFN_INVALID)
		return STATUS_INSUFFICIENT_MEMORY;
	
	QueueEntry.Sub.DataPointer[0] = MmPFNToPhysPage(Page);
	
	QueueEntry.Sub.Dword10.Identify.Cns = Cns;
	
	BSTATUS Status = NvmeSendAndWait(&ContExtension->AdminQueue, &QueueEntry);
	if (FAILED(Status))
		return Status;
	
	memcpy(IdentifyBuffer, MmGetHHDMOffsetAddr(MmPFNToPhysPage(Page)), PAGE_SIZE);
	
	MmFreePhysicalPage(Page);
	
	return STATUS_SUCCESS;
}

BSTATUS NvmeSetFeature(PCONTROLLER_EXTENSION ContExtension, int FeatureIdentifier, uintptr_t DataPointer)
{
	QUEUE_ENTRY_PAIR QueueEntry;
	memset(&QueueEntry, 0, sizeof QueueEntry);
	
	// note: PRP = 0, FusedOperation = 0
	QueueEntry.Sub.CommandHeader.OpCode = ADMOP_SET_FEATURES;
	
	QueueEntry.Sub.DataPointer[0] = DataPointer;
	QueueEntry.Sub.Dword10.SetFeatures.FeatureIdentifier = FeatureIdentifier;
	
	BSTATUS Status = NvmeSendAndWait(&ContExtension->AdminQueue, &QueueEntry);
	if (FAILED(Status))
		return Status;
	
	if (QueueEntry.Comp.Status.Code != 0)
		return STATUS_HARDWARE_IO_ERROR;
	
	return STATUS_SUCCESS;
}

BSTATUS NvmeAllocateIoQueues(PCONTROLLER_EXTENSION ContExtension, size_t QueueCount, size_t* OutQueueCount)
{
	ASSERT(QueueCount != 0);
	QueueCount--;
	
	QUEUE_ENTRY_PAIR QueueEntry;
	memset(&QueueEntry, 0, sizeof QueueEntry);
	
	// note: PRP = 0, FusedOperation = 0
	QueueEntry.Sub.CommandHeader.OpCode = ADMOP_SET_FEATURES;
	QueueEntry.Sub.Dword10.SetFeatures.FeatureIdentifier = NVME_FEAT_QUEUE_COUNT;
	QueueEntry.Sub.Dword11.SetFeatures.SubQueueCount = QueueCount;
	QueueEntry.Sub.Dword11.SetFeatures.ComQueueCount = QueueCount;
	
	BSTATUS Status = NvmeSendAndWait(&ContExtension->AdminQueue, &QueueEntry);
	if (FAILED(Status))
		return Status;
	
	if (QueueEntry.Comp.Status.Code != 0)
		return STATUS_HARDWARE_IO_ERROR;
	
	uint16_t Min = QueueEntry.Comp.SetFeatures.QueueCount.Sub + 1;
	if (Min > QueueEntry.Comp.SetFeatures.QueueCount.Com + 1)
		Min = QueueEntry.Comp.SetFeatures.QueueCount.Com + 1;
	
	*OutQueueCount = Min + 1;
	return STATUS_SUCCESS;
}

// Initializes a queue control block as an I/O queue. The admin queue is initialized in a different place.
BSTATUS NvmeInitializeIoQueue(PCONTROLLER_EXTENSION ContExtension, PQUEUE_CONTROL_BLOCK Qcb, size_t Id)
{
	int SubQueuePfn = MmAllocatePhysicalPage();
	int ComQueuePfn = MmAllocatePhysicalPage();
	
	if (SubQueuePfn == PFN_INVALID || ComQueuePfn == PFN_INVALID)
	{
		if (SubQueuePfn != PFN_INVALID)
			MmFreePhysicalPage(SubQueuePfn);
		if (ComQueuePfn != PFN_INVALID)
			MmFreePhysicalPage(ComQueuePfn);
		
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	QUEUE_ENTRY_PAIR QueueEntry;
	memset(&QueueEntry, 0, sizeof QueueEntry);
	// note: PRP = 0, FusedOperation = 0
	
	// Create the completion queue
	QueueEntry.Sub.CommandHeader.OpCode = ADMOP_CREATE_IO_COMPLETION_QUEUE;
	
	QueueEntry.Sub.DataPointer[0] = MmPFNToPhysPage(ComQueuePfn);
	QueueEntry.Sub.Dword10.CreateIoQueue.QueueId = Id;
	QueueEntry.Sub.Dword10.CreateIoQueue.QueueSize = COMPLETION_QUEUE_SIZE;
	QueueEntry.Sub.Dword11.CreateIoCompQueue.InterruptsEnabled = 1;
	QueueEntry.Sub.Dword11.CreateIoCompQueue.PhysicallyContiguous = 1;
	QueueEntry.Sub.Dword11.CreateIoCompQueue.InterruptVector = Id;
	
	BSTATUS Status = NvmeSendAndWait(&ContExtension->AdminQueue, &QueueEntry);
	if (FAILED(Status))
		return Status;
	
	if (QueueEntry.Comp.Status.Code != 0)
	{
		DbgPrint("StorNvme: failed to create I/O completion queue %zu", Id);
		MmFreePhysicalPage(SubQueuePfn);
		MmFreePhysicalPage(ComQueuePfn);
		return STATUS_HARDWARE_IO_ERROR;
	}
	
	// Create the submission queue
	QueueEntry.Sub.CommandHeader.OpCode = ADMOP_CREATE_IO_SUBMISSION_QUEUE;
	
	QueueEntry.Sub.DataPointer[0] = MmPFNToPhysPage(SubQueuePfn);
	QueueEntry.Sub.Dword10.AsUint32 = 0;
	QueueEntry.Sub.Dword11.AsUint32 = 0;
	QueueEntry.Sub.Dword10.CreateIoQueue.QueueId = Id;
	QueueEntry.Sub.Dword10.CreateIoQueue.QueueSize = SUBMISSION_QUEUE_SIZE;
	QueueEntry.Sub.Dword11.CreateIoSubQueue.CompletionQueueId = Id;
	QueueEntry.Sub.Dword11.CreateIoSubQueue.PhysicallyContiguous = 1;
	
	Status = NvmeSendAndWait(&ContExtension->AdminQueue, &QueueEntry);
	if (FAILED(Status))
		return Status;
	
	if (QueueEntry.Comp.Status.Code != 0)
	{
		DbgPrint("StorNvme: failed to create I/O submission queue %zu", Id);
		ASSERT(false && "TODO: Cleanup");
		MmFreePhysicalPage(SubQueuePfn);
		MmFreePhysicalPage(ComQueuePfn);
		return STATUS_HARDWARE_IO_ERROR;
	}
	
	// Now initialize the Qcb itself.
	NvmeSetupQueue(ContExtension, Qcb, MmPFNToPhysPage(SubQueuePfn), MmPFNToPhysPage(ComQueuePfn), (int) Id, (int) Id);
	
	return STATUS_SUCCESS;
}
