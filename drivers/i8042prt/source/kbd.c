/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	kbd.c
	
Abstract:
	This module implements the keyboard part of the
	i8042 device driver.
	
Author:
	iProgramInCpp - 2 April 2024
***/
#include "kbd.h"

static PDEVICE_OBJECT KbdDeviceObject;

static KINTERRUPT KbdInterrupt;
static KSPIN_LOCK KbdInterruptLock;
static KDPC KbdDpc;

static uint8_t  KbdBufferInt[4096];
static uint8_t  KbdBuffer[4096];
static uint16_t KbdPendingReads;

static KEVENT KbdAvailableEvent;

typedef struct FCB_EXTENSION
{
	uint8_t KeyCodes[256];
	int Tail, Head;
}
FCB_EXTENSION, *PFCB_EXTENSION;

#define Extension(Fcb) ((PFCB_EXTENSION) Fcb->Extension)

static PFCB_EXTENSION KbdFcbExtension;

static void KbdAddKeyToBuffer(uint8_t Code)
{
	PFCB_EXTENSION Ext = KbdFcbExtension;
	
	int NewHead = (Ext->Head + 1) % sizeof (Ext->KeyCodes);
	if (NewHead == Ext->Tail) {
		DbgPrint("Key press dropped due to full buffer");
		return;
	}
	
	Ext->KeyCodes[Ext->Head] = Code;
	Ext->Head = NewHead;
}

static uint8_t KbdReadKeyFromBuffer()
{
	PFCB_EXTENSION Ext = KbdFcbExtension;
	
	if (Ext->Head == Ext->Tail)
		return 0xFF;
	
	int NewTail = (Ext->Tail + 1) % sizeof (Ext->KeyCodes);
	uint8_t Read = Ext->KeyCodes[Ext->Tail];
	Ext->Tail = NewTail;
	
	return Read;
}

// Fetches the bytes read from the keyboard controller.
static int KbdFetchBuffer(UNUSED void* Context)
{
	// Fetch the amount of key codes and reset it to zero.
	int Result = KbdPendingReads;
	KbdPendingReads = 0;
	
	// Copy the buffer over.
#ifdef SECURE
	memcpy(KbdBuffer, KbdBufferInt, sizeof KbdBufferInt);
	memset(KbdBufferInt, 0, sizeof KbdBufferInt);
#else
	memcpy(KbdBuffer, KbdBufferInt, Result);
#endif
	
	// Return the amount of key codes that have been received.
	return Result;
}

// N.B.  This DPC can only run on the same processor as the interrupt source.
static void KbdDpcRoutine(UNUSED PKDPC Dpc, UNUSED void* Context, UNUSED void* SysArg1, UNUSED void* SysArg2)
{
	// Fetch the bytes that have been read from the keyboard controller.
	//
	// Perform this operation at the IPL of the keyboard interrupt
	// to ensure atomicity with it, and to ensure that no keyboard
	// inputs are being missed.
	int Count = KeSynchronizeExecution(&KbdInterrupt, 0, KbdFetchBuffer, NULL);
	
	for (int i = 0; i < Count; i++)
	{
		KbdAddKeyToBuffer(KbdBuffer[i]);
	}
	
#ifdef SECURE
	memset(KbdBuffer, 0, sizeof KbdBuffer);
#endif
	
	if (Count != 0)
	{
		// TODO HACK: Calling the Kernel internal version of SetEvent that assumes
		// the dispatcher lock is being held. It is, in this case.  Need a better
		// way or need to figure out why DPCs have the dispatcher lock held anyway
		extern void KiSetEvent(PKEVENT, KPRIORITY);
		
		KiSetEvent(&KbdAvailableEvent, 1);
	}
}

static void KbdInterruptRoutine(UNUSED PKINTERRUPT Interrupt, void* Context)
{
	// TODO: Is this necessary?
	uint8_t Status = KePortReadByte(I8042_PORT_STATUS);
	if (~Status & I8042_STATUS_OUTPUT_FULL)
		return;
	
	uint8_t Code = KePortReadByte(I8042_PORT_DATA);
	
	if (KbdPendingReads >= sizeof KbdBufferInt)
		// Key codes haven't been read fast enough!  Simply return as
		// there isn't really anything we can do to solve this.
		return;
	
	// Add the code to the buffer.
	KbdBufferInt[KbdPendingReads] = Code;
	KbdPendingReads++;
	
	// If there were no pending reads before this interrupt,
	// enqueue the keyboard DPC.
	if (KbdPendingReads != 1)
		return;
	
	// Initialize and enqueue the DPC now.
	KeInitializeDpc(&KbdDpc, KbdDpcRoutine, Context);
	KeSetImportantDpc(&KbdDpc, true);
	KeEnqueueDpc(&KbdDpc, NULL, NULL);
}

void KbdInitialize(int Vector, KIPL Ipl)
{
	// TODO: Maybe we should reform the KeInitializeInterrupt to actually operate on legacy IRQs?
	// Example: IrqNum IN [0, 32) -> Legacy IRQ, IrqNum | 0x10000000 -> MSI IRQ or whatever else.
	// This would avert the need to configure the IOAPIC directly in the driver.
	
	KeInitializeInterrupt(
		&KbdInterrupt,
		KbdInterruptRoutine,
		NULL,
		&KbdInterruptLock,
		Vector,
		Ipl,
		false
	);
	
	if (!KeConnectInterrupt(&KbdInterrupt))
		KeCrash("KbdInitialize: Cannot connect interrupt");
}

IO_DISPATCH_TABLE KbdDispatchTable;

BSTATUS KbdRead(
	PIO_STATUS_BLOCK Iosb,
	UNUSED PFCB Fcb,
	UNUSED uintptr_t Offset,
	size_t Length,
	void* Buffer,
	bool Block
)
{
	ASSERT(Extension(Fcb) == KbdFcbExtension);
	
	uint8_t* BufferBytes = Buffer;
	
	for (size_t i = 0; i < Length; )
	{
		BufferBytes[i] = KbdReadKeyFromBuffer();
		
		if (BufferBytes[i] != 0xFF)
		{
			i++;
			continue;
		}
		
		if (!Block)
		{
			// Can't block, so return now.
			Iosb->Status = STATUS_SUCCESS;
			Iosb->BytesRead = i;
			return STATUS_SUCCESS;
		}
		
		// Wait.
		BSTATUS Status = KeWaitForSingleObject(&KbdAvailableEvent, true, TIMEOUT_INFINITE);
		
		if (Status != STATUS_SUCCESS)
		{
			Iosb->BytesRead = i;
			Iosb->Status = Status;
			return Status;
		}
		
		// The read will be re-tried.
	}
	
	Iosb->BytesRead = Length;
	Iosb->Status = STATUS_SUCCESS;
	
	return STATUS_SUCCESS;
}

void KbdInitializeDispatchTable()
{
	KbdDispatchTable.Read = &KbdRead;
	
	KbdDispatchTable.Flags = DISPATCH_FLAG_EXCLUSIVE;
}

extern PDRIVER_OBJECT I8042DriverObject;

BSTATUS KbdCreateDeviceObject()
{
	BSTATUS Status;
	
	KbdInitializeDispatchTable();
	
	KeInitializeEvent(
		&KbdAvailableEvent,
		EVENT_SYNCHRONIZATION,
		false
	);
	
	Status = IoCreateDevice(
		I8042DriverObject,
		0,                      // DeviceExtensionSize
		sizeof (FCB_EXTENSION), // FcbExtensionSize
		"I8042PrtKeyboard",
		DEVICE_TYPE_CHARACTER,
		false,
		&KbdDispatchTable,
		&KbdDeviceObject
	);
	
	if (FAILED(Status))
	{
		DbgPrint("IoCreateDevice for I8042prt Keyboard failed: %d", Status);
		return Status;
	}
	
	KbdFcbExtension = Extension(KbdDeviceObject->Fcb);
	
	return Status;
}
