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
#include <hal.h>

#define MEASURE_LATENCIES

static PDEVICE_OBJECT KbdDeviceObject;

static KINTERRUPT KbdInterrupt;
static KSPIN_LOCK KbdInterruptLock;
static KDPC KbdDpc;

static uint8_t  KbdBufferInt[4096];
static uint8_t  KbdBuffer[4096];
static uint16_t KbdPendingReads;

#ifdef MEASURE_LATENCIES
static uint64_t kbdLastInterruptTick = 0;
static uint64_t kbdLastDpcTick = 0;
#endif

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
	
#ifdef MEASURE_LATENCIES
	kbdLastDpcTick = HalGetTickCount();
#endif
	
#ifdef SECURE
	memset(KbdBuffer, 0, sizeof KbdBuffer);
#endif
	
	if (Count != 0)
	{
		KeSetEvent(&KbdAvailableEvent, 1);
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
	// TODO: Don't think we should reinitialize the DPC every time
	KeInitializeDpc(&KbdDpc, KbdDpcRoutine, Context);
	KeSetImportantDpc(&KbdDpc, true);
	KeEnqueueDpc(&KbdDpc, NULL, NULL);
	
#ifdef MEASURE_LATENCIES
	kbdLastInterruptTick = HalGetTickCount();
#endif
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
	
	// Clear the keyboard input queue.
	while (KePortReadByte(I8042_PORT_STATUS) & I8042_STATUS_OUTPUT_FULL)
		KePortReadByte(I8042_PORT_DATA);
}

BSTATUS KbdRead(
	PIO_STATUS_BLOCK Iosb,
	UNUSED PFCB Fcb,
	UNUSED uintptr_t Offset,
	PMDL Mdl,
	uint32_t Flags
)
{
	ASSERT(Extension(Fcb) == KbdFcbExtension);
	
	void* Buffer = NULL;
	BSTATUS Status;
	Status = MmMapPinnedPagesMdl(Mdl, &Buffer);
	if (FAILED(Status))
	{
		Iosb->Status = Status;
		return Status;
	}
	
	size_t Length = Mdl->ByteCount;
	
	uint8_t* BufferBytes = Buffer;
	
	for (size_t i = 0; i < Length; )
	{
		BufferBytes[i] = KbdReadKeyFromBuffer();
		
		if (BufferBytes[i] != 0xFF)
		{
		#ifdef MEASURE_LATENCIES
			uint64_t tickCount = HalGetTickCount();
			DbgPrint(
				"latency: %lld ticks since last interrupt (%lld since last DPC, %lld ticks between int and dpc), clock ticking at %lld ticks/s", 
				tickCount - kbdLastInterruptTick,
				tickCount - kbdLastDpcTick,
				kbdLastDpcTick - kbdLastInterruptTick,
				HalGetTickFrequency()
			);
		#endif
			i++;
			continue;
		}
		
		if (Flags & IO_RW_NONBLOCK)
		{
			// Can't block, so return now.
			Iosb->Status = STATUS_SUCCESS;
			Iosb->BytesRead = i;
			return STATUS_SUCCESS;
		}
		
		BSTATUS Status = KeWaitForSingleObject(&KbdAvailableEvent, true, TIMEOUT_INFINITE, MODE_KERNEL);
		
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
	
	//MmUnmapPagesMdl(Mdl);
	
	return STATUS_SUCCESS;
}

bool KbdSeekable(UNUSED PFCB Fcb)
{
	return false;
}

IO_DISPATCH_TABLE KbdDispatchTable = {
	.Read = &KbdRead,
	.Seekable = &KbdSeekable,
	.Flags = DISPATCH_FLAG_EXCLUSIVE,
};

extern PDRIVER_OBJECT I8042DriverObject;

BSTATUS KbdCreateDeviceObject()
{
	BSTATUS Status;
	
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
