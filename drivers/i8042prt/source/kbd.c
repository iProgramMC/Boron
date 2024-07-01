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
		uint8_t Code = KbdBuffer[i];
		
		// Print all key codes obtained
		DbgPrint("Got key code (%d): %02x", i, Code);
		
		// TODO: Add them into the IO device's buffer here...
	}
	
#ifdef SECURE
	memset(KbdBuffer, 0, sizeof KbdBuffer);
#endif
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
	UNUSED size_t Length,
	UNUSED void* Buffer,
	UNUSED bool Block
)
{
	// TODO
	Iosb->Status = STATUS_UNIMPLEMENTED;
	
	return STATUS_UNIMPLEMENTED;
}

void KbdInitializeDispatchTable()
{
	KbdDispatchTable.Read = &KbdRead;
}

extern PDRIVER_OBJECT I8042DriverObject;

BSTATUS KbdCreateDeviceObject()
{
	BSTATUS Status;
	
	KbdInitializeDispatchTable();
	
	Status = IoCreateDevice(
		I8042DriverObject,
		0, // DeviceExtensionSize
		0, // FcbExtensionSize
		"I8042PrtKeyboard",
		DEVICE_TYPE_CHARACTER,
		false,
		&KbdDispatchTable,
		&KbdDeviceObject
	);
	
	if (FAILED(Status))
		DbgPrint("IoCreateDevice for I8042prt Keyboard failed: %d", Status);
	
	return Status;
}
