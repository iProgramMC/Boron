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

int MtModel = 0;
const char* MtFilePath = "/Root/etc/zephyr2.bin";
const char* MtClPath = "/Devices/HardwareConfiguration/MtCl";
const char* PxClPath = "/Devices/HardwareConfiguration/PxCl";

#ifdef CONFIG_SMP
#error TODO: synchronize with KeSynchronizeExecution instead of disabling interrupts!
#endif

// TODO: The current driver is basically a rewrite of OpeniBoot's multitouch-z2.c driver.
//
// Its quality is very questionable.
// It needs to be rewritten to fit better into the rest of the OS.

KINTERRUPT MtInterrupt;
KSPIN_LOCK MtInterruptSpinLock; // KeSynchronizeExecution
KDPC MtDpc;
int MtPendingAtnEvents;
KEVENT MtPendingAtnEventsEvent;
FAST_MUTEX MtPendingMutex;

void *FirmwareBuffer, *ProxCalBuffer, *MtCalBuffer;
size_t FirmwareLength, ProxCalLength, MtCalLength;

void MtDpcRoutine(PKDPC Dpc UNUSED, void* Context UNUSED, void* SA1 UNUSED, void* SA2 UNUSED)
{
	ASSERT(Dpc == &MtDpc);
	KeSetEvent(&MtPendingAtnEventsEvent, MULTITOUCH_INCREMENT);
}

void MtInterruptRoutine(PKINTERRUPT Interrupt UNUSED, void* Context UNUSED)
{
	DbgPrint("Interrupt!");
	ASSERT(Interrupt == &MtInterrupt);

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

bool MtCheckForEvents(bool Wait, int TimeoutMS)
{
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
}

uint16_t MtPerformHbppAtnAck()
{
	uint8_t Tx[2], Rx[2];

	uint64_t StartTime = HalGetTickCount();

	// wait 100 milliseconds to see if we have an event.
	bool HaveEvent = MtCheckForEvents(true, 100);
	if (!HaveEvent)
	{
		// no event.
		return 0xFFFF;
	}

	Tx[0] = 0x1A;
	Tx[1] = 0xA1;

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

	uint16_t RV = (Rx[0] << 8) | Rx[1];
	//LogMsg("zephyr2: MtPerformHbppAtnAck returned 0x%04x", RV);
	return RV;
}

bool MtAttemptUploadFirmware()
{
	const int Tries = 5;
	for (int Try = 0; Try < Tries; Try++)
	{
		LogMsg("zephyr2: Uploading firmware... (attempt %d/%d)", Try + 1, Tries);

		// GotATN = 0;

		int ReturnValue = MtSpiTx(&MtFastSpeed, FirmwareBuffer, FirmwareLength);
		if ((size_t) ReturnValue != FirmwareLength)
			LogMsg("zephyr2: ERROR: MiSpiTx value is %d, expected %zu", ReturnValue, FirmwareLength);

		if (MtPerformHbppAtnAck() == 0x4BC1)
			return true;
	}

	return false;
}

void MtSetupMultitouchPart2()
{
	for (int i = 0; i < 4; i++)
	{
		HalSetOutputPinGpio(MtGpioUnknown606(), 0);
		OSSleep(200);
		HalSetOutputPinGpio(MtGpioUnknown606(), 1);
		OSSleep(15);

		LogMsg("zephyr2: Acknowledging initial events...");
		while (MtPerformHbppAtnAck() != 0xFFFF)
		{
			KeSpinningHint();
		}

		LogMsg("zephyr2: Trying to upload firmware...");
		if (MtAttemptUploadFirmware())
		{
			LogMsg("zephyr2: Firmware uploaded successfully.");
			return;
		}
	}

	LogMsg("zephyr2: Failed to upload firmware.");

	if (!MtCalBuffer) {
		LogMsg("zephyr2: Bypassing setup procedure. (you might be running on an emulator)");
	}
	else {
		LogMsg("zephyr2: Multitouch will be inoperable for this session.");
	}
}

static BSTATUS ReadEntireFile(const char* Path, void** OutBuffer, size_t* OutSize, uint32_t CreateOptions)
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
void FirmwareUploadThread(UNUSED void* Context)
{
	// TODO: wait for file systems to be mounted, first.
	OSSleep(100);

	BSTATUS Status;
	Status = ReadEntireFile(MtFilePath, &FirmwareBuffer, &FirmwareLength, IO_FILE_REGULAR_FILE);
	if (FAILED(Status))
	{
		LogMsg("zephyr2: Could not load %s: %s", MtFilePath, RtlGetStatusString(Status));
		// TODO: Request unload
		OSExitThread();
	}

	if (MtModel == N45AP)
	{
		ProxCalBuffer = NULL;
		ProxCalLength = 0;
	}
	else
	{
		// N82 requests it and I think N72 will as well?
		Status = ReadEntireFile(PxClPath, &ProxCalBuffer, &ProxCalLength, IO_FILE_ANY_TYPE);
		if (FAILED(Status))
		{
			MmFreePool(FirmwareBuffer);
			LogMsg("zephyr2: Could not load %s: %s", PxClPath, RtlGetStatusString(Status));
			OSExitThread();
		}
	}

	// But Multitouch Calibration data is required
	Status = ReadEntireFile(MtClPath, &MtCalBuffer, &MtCalLength, IO_FILE_ANY_TYPE);
	if (FAILED(Status))
	{
		MtCalBuffer = NULL;
		MtCalLength = 0;
		LogMsg("MtCl not found. It's possible you're running this on an emulator.");
	}

	MtSetupMultitouchPart2();

	OSExitThread();
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

	PETHREAD Thread;
	BSTATUS Status = PsCreateSystemThreadFast(
		&Thread,
		FirmwareUploadThread,
		NULL, // StartContext
		false // CreateSuspended
	);

	if (FAILED(Status))
	{
		LogMsg("zephyr2: Cannot create upload thread: %s", RtlGetStatusString(Status));
		return STATUS_UNLOAD;
	}

	ObDereferenceObject(Thread);

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
