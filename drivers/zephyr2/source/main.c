/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	main.c

Abstract:
	This module implements the main function of the zephyr2
	multitouch driver.

Author:
	iProgramInCpp - 26 October 2026
***/
#include "z2.h"

#define MULTITOUCH_INCREMENT 5

//#define BUSY_WAIT

int MtModel = 0;
#ifdef CONFIG_SMP
#error TODO: synchronize with KeSynchronizeExecution instead of disabling interrupts!
#endif

// NOTE: The emulator in question is my fork of qemu-ios by devos50,
// available at https://github.com/iProgramMC/qemu-ios
volatile bool MtInitialized = false;
volatile bool MtEmulatorMode = false;

uint32_t MtInterfaceVersion;
uint32_t MtMaxPacketSize;
bool MtFlipNop;
uint32_t MtCurNop;

uint32_t MtSensorWidth;
uint32_t MtSensorHeight;

// TODO: The current driver is basically a rewrite of OpeniBoot's multitouch-z2.c driver.
//
// Its quality is very questionable.
// It needs to be rewritten to fit better into the rest of the OS.

KINTERRUPT MtInterrupt;
KSPIN_LOCK MtInterruptSpinLock; // KeSynchronizeExecution
KDPC MtDpc;
volatile int MtPendingAtnEvents;
KEVENT MtPendingAtnEventsEvent;
FAST_MUTEX MtPendingMutex;

void MtDpcRoutine(PKDPC Dpc UNUSED, void* Context UNUSED, void* SA1 UNUSED, void* SA2 UNUSED)
{
	DbgPrint("MtDpcRoutine");

	ASSERT(Dpc == &MtDpc);
	KeSetEvent(&MtPendingAtnEventsEvent, MULTITOUCH_INCREMENT);
}

void MtInterruptRoutine(PKINTERRUPT Interrupt UNUSED, void* Context UNUSED)
{
	ASSERT(Interrupt == &MtInterrupt);

	DbgPrintString("MtInterruptRoutine\n");

	AtAddFetch(MtPendingAtnEvents, 1);
	KeEnqueueDpc(
		&MtDpc,
		NULL,
		NULL
	);
}

void MtPowerMultitouch()
{
	HalSetOutputPinGpio(MtGpioPower(), 0);
	OSSleep(200);
	HalSetOutputPinGpio(MtGpioPower(), 1);
	OSSleep(15);
}

uint16_t MtChecksum(uint8_t* Buffer, size_t Start, size_t End)
{
	uint16_t CheckSum = 0;
	for (size_t i = Start; i < End; i++)
		CheckSum += Buffer[i];

	return CheckSum;
}

uint32_t MtChecksum32(uint8_t* Buffer, size_t Start, size_t End)
{
	uint32_t CheckSum = 0;
	for (size_t i = Start; i < End; i++)
		CheckSum += Buffer[i];

	return CheckSum;
}

bool MtCheckForEvents(bool Wait, int TimeoutMS)
{
#ifndef BUSY_WAIT
	// This mutex is there so that nobody else can check for events before us.
	ExAcquireFastMutex(&MtPendingMutex, false);

	if (!MtPendingAtnEvents)
	{
		// No events.
		if (!Wait) {
			ExReleaseFastMutex(&MtPendingMutex);
			return false;
		}

		BSTATUS Status = KeWaitForSingleObject(
			&MtPendingAtnEventsEvent,
			false, // Alertable
			TimeoutMS,
			MODE_KERNEL // WaitMode
		);

		if (FAILED(Status)) {
			if (Status != STATUS_TIMEOUT)
				DbgPrint("Failed to wait on ATN event: %s", RtlGetStatusString(Status));
			ExReleaseFastMutex(&MtPendingMutex);
			return false;
		}
	}

	KIPL Ipl = KeRaiseIPL(IPL_DPC);

	int NewValue = AtAddFetch(MtPendingAtnEvents, -1);
	if (NewValue == 0) {
		KeResetEvent(&MtPendingAtnEventsEvent);
	}

	KeLowerIPL(Ipl);

	ExReleaseFastMutex(&MtPendingMutex);
	return true;
#else // BUSY_WAIT
	uint64_t StartTime = HalGetTickCount();
	uint64_t EndTime = 0;

	if (TimeoutMS != TIMEOUT_INFINITE) {
		EndTime = StartTime + TimeoutMS * HalGetTickFrequency() / 1000;
	}

	// TODO: Don't spinwait here
	while (!MtPendingAtnEvents) {
		KeSpinningHint();
		if (TimeoutMS != TIMEOUT_INFINITE) {
			if (HalGetTickCount() > EndTime) {
				return false;
			}
		}
	}
	AtAddFetch(MtPendingAtnEvents, -1);
	return true;
#endif
}

uint16_t MtPerformHbppAtnAck()
{
	uint8_t Tx[2], Rx[2];

	// wait 100 milliseconds to see if we have an event.
	bool HaveEvent = MtCheckForEvents(true, 100);
	if (!HaveEvent)
	{
		// no event.
		//LogMsg("zephyr2: MtPerformHbppAtnAck failed");
		return 0xFFFF;
	}

	Tx[0] = 0x1A;
	Tx[1] = 0xA1;

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

	uint16_t RV = (Rx[0] << 8) | Rx[1];
	//LogMsg("zephyr2: MtPerformHbppAtnAck returned 0x%04x", RV);
	return RV;
}

uint32_t MtPerformHbppLongAtnAck()
{
	uint8_t Tx[8], Rx[8];

	// wait 100 milliseconds to see if we have an event.
	bool HaveEvent = MtCheckForEvents(true, 1000);
	if (!HaveEvent)
	{
		// no event.
		//LogMsg("zephyr2: MtPerformHbppLongAtnAck failed");
		return 0xFFFF;
	}

	Tx[0] = 0x1A;
	Tx[1] = 0xA1;
	Tx[2] = 0x18;
	Tx[3] = 0xE1;
	Tx[4] = 0x18;
	Tx[5] = 0xE1;
	Tx[6] = 0x18;
	Tx[7] = 0xE1;

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

	uint32_t Rv = 0;
	Rv |= (uint32_t) Rx[2] << 8;
	Rv |= (uint32_t) Rx[3] << 0;
	Rv |= (uint32_t) Rx[4] << 24;
	Rv |= (uint32_t) Rx[5] << 16;
	return Rv;
}

BSTATUS ReadEntireFile(const char* Path, void** OutBuffer, size_t* OutSize, uint32_t CreateOptions)
{
	BSTATUS Status;
	HANDLE Handle;
	OBJECT_ATTRIBUTES Attributes;
	IO_STATUS_BLOCK Iosb;

	memset(&Attributes, 0, sizeof Attributes);
	Attributes.OpenFlags = 0;
	Attributes.ObjectName = Path;
	Attributes.ObjectNameLength = strlen(Attributes.ObjectName);
	Attributes.RootDirectory = HANDLE_NONE;

	Status = OSCreateFile(
		&Handle,
		&Attributes,
		&Iosb,
		0,    // FileAttributes
		0,    // AllocationSize
		IO_OPEN,
		CreateOptions,
		NULL, // TargetName
		0     // TargetNameLength
	);

	if (FAILED(Status))
		return Status;

	uint64_t FileLength64 = 0;
	Status = OSGetLengthFile(Handle, &FileLength64);
	if (FAILED(Status))
	{
		OSClose(Handle);
		return Status;
	}

	if (FileLength64 < 1 || FileLength64 > 10000000)
	{
		LogMsg("zephyr2: %s has a bogus file length: %s", Attributes.ObjectName, RtlGetStatusString(Status));
		OSClose(Handle);
		return STATUS_INSUFFICIENT_MEMORY;
	}

	size_t Length = (size_t) FileLength64;
	void* Buffer = MmAllocatePool(POOL_NONPAGED, Length);
	if (!Buffer)
	{
		LogMsg("zephyr2: Cannot allocate %zu bytes for firmware buffer.", Length);
		OSClose(Handle);
		return STATUS_INSUFFICIENT_MEMORY;
	}

	Status = OSReadFile(
		&Iosb,
		Handle,
		0, // ByteOffset
		Buffer,
		Length,
		0  // Flags
	);

	if (IOSUCCEEDED(Status) && Iosb.BytesRead < Length)
	{
		LogMsg("Warning: read %zu bytes, expected %zu bytes", Iosb.BytesRead, Length);
		memset((uint8_t*)Buffer + Iosb.BytesRead, 0, Length - Iosb.BytesRead);
	}

	if (FAILED(Status))
	{
		OSClose(Handle);
		MmFreePool(Buffer);
		return Status;
	}

	*OutBuffer = Buffer;
	*OutSize = Length;
	return Status;
}

NO_RETURN
static void MtMultitouchThread(UNUSED void* Context)
{
	// TODO: wait for file systems to be mounted, first.
	OSSleep(100);

	if (!MtPrepareInitialData())
	{
		OSExitThread();
	}

	if (!MtSetupMultitouchDevice())
	{
		OSExitThread();
	}

	MtRunMultitouch();

	OSExitThread();
}

// Prepare the multitouch thread for execution.  Once file systems are mounted,
// this will kick in and upload the firmware to the device, and then wait on the
// device to read data from it.
bool MtSetupMultitouchThread()
{
	PETHREAD Thread;
	BSTATUS Status = PsCreateSystemThreadFast(
		&Thread,
		MtMultitouchThread,
		NULL, // StartContext
		false // CreateSuspended
	);

	if (FAILED(Status))
	{
		LogMsg("zephyr2: Cannot create multitouch thread: %s", RtlGetStatusString(Status));
		return false;
	}

	ObDereferenceObject(Thread);

	return true;
}

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	KeInitializeEvent(&MtPendingAtnEventsEvent, EVENT_NOTIFICATION, false);
	KeInitializeDpc(
		&MtDpc,
		MtDpcRoutine,
		NULL // DeferredContext
	);
	ExInitializeFastMutex(&MtPendingMutex);

	// Identify the current model.
	const char* Id = KeLoaderParameterBlock.HardwareIdentifier;
	if (strcmp(Id, "n45ap") == 0) {
		MtModel = N45AP;
	}
	else if (strcmp(Id, "n82ap") == 0) {
		MtModel = N82AP;
	}
	else if (strcmp(Id, "n72ap") == 0) {
		MtModel = N72AP;
	}
	else {
		LogMsg("Z2: Unknown device model '%s'.  Supported: N45, N82, N72.");
		return STATUS_UNLOAD;
	}

	// Connect and enable the interrupt.
	HalRegisterGpioInterrupt(
		MtAtnInterruptVector(),
		false, // TriggerEdge
		false, // Level
		false  // FlipLevel
	);

	KeInitializeInterrupt(
		&MtInterrupt,
		&MtInterruptRoutine,
		NULL, // ServiceContext
		&MtInterruptSpinLock,
		APPLE_IRQ_NUMBER_GPIO_TO_NORMAL(MtAtnInterruptVector()),
		IPL_DEVICES0,
		false  // SharedVector
	);

	bool Result UNUSED = KeConnectInterrupt(&MtInterrupt);
	ASSERT(Result && "Could not connect Multitouch interrupt successfully.");

	HalEnableGpioInterrupt(MtAtnInterruptVector());

	MtPowerMultitouch();

	if (!MtSetupMultitouchThread())
	{
		return STATUS_UNLOAD;
	}

	return STATUS_SUCCESS;
}

// Port selection, based on hardware model
int MtGpioPower()
{
	return MT_GPIO_POWER;
}

int MtGpioUnknown606()
{
	return MT_GPIO_UNKNOWN_606;
}

int MtAtnInterruptVector()
{
	switch (MtModel)
	{
		case N45AP:
		case N82AP:
		default:
			return MT_ATN_INTERRUPT;
		case N72AP:
			return MT_ATN_INTERRUPT_N72;
	}
}

int MtSpiPort()
{
	switch (MtModel)
	{
		default:
		case N45AP: return MT_SPI_N45;
		case N82AP: return MT_SPI_N82;
		case N72AP: return MT_SPI_N72;
	}
}

int MtSpiCsPin()
{
	switch (MtModel)
	{
		default:
		case N45AP: return MT_SPI_CS_N45;
		case N82AP: return MT_SPI_CS_N82;
		case N72AP: return MT_SPI_CS_N72;
	}
}
