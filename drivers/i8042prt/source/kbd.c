/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the keyboard part of the
	i8042 device driver.
	
Author:
	iProgramInCpp - 2 April 2024
***/
#include "i8042.h"

static KINTERRUPT KbdInterrupt;
static KSPIN_LOCK KbdInterruptLock;
static KDPC KbdDpc;
static bool KbdDpcEnqueued;

static uint8_t KbdBuffer[4096];
static int KbdTail, KbdHead;
static bool KbdHaveData;

static uint8_t KbdDpcBuffer[4096];
static int KbdDpcCount;

// Only the interrupt handler should interact with this.
static bool AddToBuffer(uint8_t data)
{
	int NewHead = (KbdHead + 1) % sizeof KbdBuffer;
	if (NewHead == KbdTail)
	{
		// Oops, head and tail crashed into each other.  Drop this byte
		return false;
	}
	
	KbdBuffer[KbdHead] = data;
	KbdHaveData = true;
	
	// Advance head.
	KbdHead = NewHead;
	return true;
}

// Only the synchronize routine should interact with this.
static int GetFromBuffer()
{
	if (KbdHead == KbdTail)
		return -1;
	
	uint8_t Byte = KbdBuffer[KbdTail];
	
	// Advance tail.
	KbdTail = (KbdTail + 1) % sizeof KbdBuffer;
	
	// If the tail is on top of the head, then there is no more data.
	if (KbdHead == KbdTail)
		KbdHaveData = false;
	
	return (int)Byte;
}

// Note: This also allows another DPC to be fired.
// The DPC routine doesn't use the DPC object anyway, so it's fine.
static int KbdFetchBuffer(UNUSED void* Context)
{
	KbdDpcCount = 0;
	int Byte;
	while ((Byte = GetFromBuffer()) >= 0)
		KbdDpcBuffer[KbdDpcCount++] = (uint8_t)Byte;
	
	KbdDpcEnqueued = false;
	return KbdDpcCount;
}

static void KbdDpcRoutine(UNUSED PKDPC Dpc, UNUSED void* Context, UNUSED void* SysArg1, UNUSED void* SysArg2)
{
	// Fetch the bytes from the buffer safely into KbdDpcBuffer.
	int Count = KeSynchronizeExecution(&KbdInterrupt, 0, KbdFetchBuffer, NULL);
	
	for (int i = 0; i < Count; i++)
	{
		uint8_t Code = KbdDpcBuffer[i];
		
		// Print all key codes obtained
		DbgPrint("Got key code (%d): %02x", i, Code);
		
		// TODO: Add them into the IO device's buffer here...
	}
}

static void KbdInterruptRoutine(UNUSED PKINTERRUPT Interrupt, void* Context)
{
	// TODO: Is this necessary?
	uint8_t Status = KePortReadByte(I8042_PORT_STATUS);
	if (~Status & I8042_STATUS_OUTPUT_FULL)
		return;
	
	uint8_t Data = KePortReadByte(I8042_PORT_DATA);
	if (!AddToBuffer(Data))
		return;
	
	if (KbdDpcEnqueued)
		return;
	
	// Initialize and enqueue the DPC now.
	KeInitializeDpc(&KbdDpc, KbdDpcRoutine, Context);
	KeSetImportantDpc(&KbdDpc, true);
	KeEnqueueDpc(&KbdDpc, NULL, NULL);
	KbdDpcEnqueued = true;
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
