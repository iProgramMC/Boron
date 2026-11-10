/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	main.c

Abstract:
	This module implements the main function of the zephyr1
	multitouch driver.

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 10 November 2026
***/
#include "z1.h"

#define MULTITOUCH_INCREMENT 5

//#define BUSY_WAIT

int MtModel = 0;
#ifdef CONFIG_SMP
#error TODO: synchronize with KeSynchronizeExecution instead of disabling interrupts!
#endif

volatile bool MtInitialized = false;

uint32_t MtInterfaceVersion;
uint32_t MtMaxPacketSize;
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
	ASSERT(Dpc == &MtDpc);
	KeSetEvent(&MtPendingAtnEventsEvent, MULTITOUCH_INCREMENT);
}

void MtInterruptRoutine(PKINTERRUPT Interrupt UNUSED, void* Context UNUSED)
{
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
	HalSetOutputPinGpio(MT_GPIO_POWER, 0);
	OSSleep(200);
	HalSetOutputPinGpio(MT_GPIO_POWER, 1);
	OSSleep(15);
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
		LogMsg("zephyr1: %s has a bogus file length: %s", Attributes.ObjectName, RtlGetStatusString(Status));
		OSClose(Handle);
		return STATUS_INSUFFICIENT_MEMORY;
	}

	size_t Length = (size_t) FileLength64;
	void* Buffer = MmAllocatePool(POOL_NONPAGED, Length);
	if (!Buffer)
	{
		LogMsg("zephyr1: Cannot allocate %zu bytes for firmware buffer.", Length);
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
		LogMsg("zephyr1: Cannot create multitouch thread: %s", RtlGetStatusString(Status));
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
	if (strcmp(Id, "m68ap") != 0) {
		LogMsg("Z1: Unknown device model '%s'.  Supported: m68ap.");
		return STATUS_UNLOAD;
	}

	// Connect and enable the interrupt.
	HalRegisterGpioInterrupt(
		MT_ATN_INTERRUPT,
		false, // TriggerEdge
		false, // Level
		false  // FlipLevel
	);

	KeInitializeInterrupt(
		&MtInterrupt,
		&MtInterruptRoutine,
		NULL, // ServiceContext
		&MtInterruptSpinLock,
		APPLE_IRQ_NUMBER_GPIO_TO_NORMAL(MT_ATN_INTERRUPT),
		IPL_DEVICES0,
		false  // SharedVector
	);

	bool Result UNUSED = KeConnectInterrupt(&MtInterrupt);
	ASSERT(Result && "Could not connect Multitouch interrupt successfully.");

	HalEnableGpioInterrupt(MT_ATN_INTERRUPT);

	MtPowerMultitouch();

	if (!MtSetupMultitouchThread())
	{
		return STATUS_UNLOAD;
	}

	return STATUS_SUCCESS;
}
