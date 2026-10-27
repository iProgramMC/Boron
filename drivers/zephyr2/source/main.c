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

#define PROX_CALIBRATION_ADDRESS 0x400180
#define MT_CALIBRATION_ADDRESS   0x400200

int MtModel = 0;
const char* MtFilePath = "/Root/etc/zephyr2.bin";
const char* MtClPath = "/Devices/HardwareConfiguration/MtCl";
const char* PxClPath = "/Devices/HardwareConfiguration/PxCl";

#ifdef CONFIG_SMP
#error TODO: synchronize with KeSynchronizeExecution instead of disabling interrupts!
#endif

static uint8_t OutputPacket[512];
static uint8_t InputPacket[512];
static uint8_t GetInfoPacket[512];
static uint8_t GetResultPacket[512];

static uint32_t MtInterfaceVersion;
static uint32_t MtMaxPacketSize;
static bool MtFlipNop;
static uint32_t MtCurNop;

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

void *MtFamilyIdBuffer, *MtSensorInfoBuffer, *MtSensorRegionDescBuffer, *MtSensorRegionParamBuffer, *MtSensorDimBuffer;
size_t MtFamilyIdLength, MtSensorInfoLength, MtSensorRegionDescLength, MtSensorRegionParamLength, MtSensorDimLength;

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

	// wait 100 milliseconds to see if we have an event.
	bool HaveEvent = MtCheckForEvents(true, 100);
	if (!HaveEvent)
	{
		// no event.
		LogMsg("zephyr2: MtPerformHbppAtnAck failed");
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
		LogMsg("zephyr2: MtPerformHbppLongAtnAck failed");
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

	LogMsg("received: %02x %02x %02x %02x %02x %02x %02x %02x", Rx[0],Rx[1],Rx[2],Rx[3],Rx[4],Rx[5],Rx[6],Rx[7]);

	uint32_t Rv = 0;
	Rv |= (uint32_t) Rx[2] << 8;
	Rv |= (uint32_t) Rx[3] << 0;
	Rv |= (uint32_t) Rx[4] << 24;
	Rv |= (uint32_t) Rx[5] << 16;
	return Rv;
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
			LogMsg("zephyr2: ERROR: MtSpiTx value is %d, expected %zu", ReturnValue, FirmwareLength);

		if (MtPerformHbppAtnAck() == 0x4BC1)
			return true;
	}

	return false;
}

static void MakeBootloaderDataPacket(uint8_t* OutputBuffer, uint32_t TargetAddress, const uint8_t* Data, size_t DataLen, int* ChecksumOut)
{
	uint32_t Checksum;

	OutputBuffer[0] = 0x30;
	OutputBuffer[1] = 0x01;
	OutputBuffer[2] = ((DataLen >> 2) >> 10) & 0xFF;
	OutputBuffer[3] = (DataLen >> 2) & 0xFF;
	OutputBuffer[4] = (TargetAddress >> 8) & 0xFF;
	OutputBuffer[5] = TargetAddress & 0xFF;
	OutputBuffer[6] = (TargetAddress >> 24) & 0xFF;
	OutputBuffer[7] = (TargetAddress >> 16) & 0xFF;

	Checksum = 0;
	for (int i = 2; i < 8; i++) {
		Checksum += OutputBuffer[i];
	}

	OutputBuffer[8] = (Checksum >> 8) & 0xFF;
	OutputBuffer[9] = Checksum & 0xFF;

	for (size_t i = 0; i < DataLen; i += 4)
	{
		OutputBuffer[10 + i + 0] = Data[i + 1];
		OutputBuffer[10 + i + 1] = Data[i + 0];
		OutputBuffer[10 + i + 2] = Data[i + 3];
		OutputBuffer[10 + i + 3] = Data[i + 2];
	}

	Checksum = 0;
	for (size_t i = 10; i < (DataLen + 10); i++) {
		Checksum += OutputBuffer[i];
	}

	OutputBuffer[DataLen + 10] = (Checksum >> 8) & 0xFF;
	OutputBuffer[DataLen + 11] = Checksum & 0xFF;
	OutputBuffer[DataLen + 12] = (Checksum >> 24) & 0xFF;
	OutputBuffer[DataLen + 13] = (Checksum >> 16) & 0xFF;

	if (ChecksumOut) {
		*ChecksumOut = Checksum;
	}
}

bool MtAttemptUploadCalibrationData(void* Buffer, size_t Length, uint32_t Address)
{
	const uint8_t* Data = Buffer;
	size_t Left = (Length + 3) & ~0x3;

	while (Left > 0)
	{
		size_t ToUpload = Left;
		if (ToUpload > 0x3F0)
			ToUpload = 0x3F0;

		OutputPacket[0] = 0x18;
		OutputPacket[1] = 0xE1;

		MakeBootloaderDataPacket(OutputPacket + 2, Address, Data, ToUpload, NULL);

		const int Tries = 5;
		int Try = 0;
		for (Try = 0; Try < Tries; Try++)
		{
			LogMsg("zephyr2: uploading calibration data (attempt %d/%d)", Try + 1, Tries);

			int ReturnValue = MtSpiTx(&MtFastSpeed, OutputPacket, ToUpload + 0x10);
			if ((size_t) ReturnValue != ToUpload + 0x10)
				LogMsg("zephyr2: ERROR: MtSpiTx value is %d, expected %zu", ReturnValue, ToUpload + 0x10);

			if (MtPerformHbppAtnAck() == 0x4BC1)
				break;
		}

		if (Try == 5)
			return false;

		Address += ToUpload;
		Data += ToUpload;
		Left -= ToUpload;
	}

	return true;
}

uint32_t MtReadRegister(uint32_t Address)
{
	uint8_t Tx[8], Rx[8];
	uint32_t CheckSum = 0;

	Tx[0] = 0x1C;
	Tx[1] = 0x73;
	Tx[2] = (Address >> 8) & 0xFF;
	Tx[3] = Address & 0xFF;
	Tx[4] = (Address >> 24) & 0xFF;
	Tx[5] = (Address >> 16) & 0xFF;

	for (int i = 2; i < 6; i++)
		CheckSum += Tx[i];

	Tx[6] = (CheckSum >> 8) & 0xFF;
	Tx[7] = CheckSum & 0xFF;

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

	return MtPerformHbppLongAtnAck();
}

bool MtWriteRegister(uint32_t Address, uint32_t Value, uint32_t Mask)
{
	uint8_t Tx[16], Rx[16];
	uint32_t CheckSum = 0;

	Tx[0] = 0x1E;
	Tx[1] = 0x33;
	Tx[2] = (Address >> 8) & 0xFF;
	Tx[3] = Address & 0xFF;
	Tx[4] = (Address >> 24) & 0xFF;
	Tx[5] = (Address >> 16) & 0xFF;

	Tx[6] = (Mask >> 8) & 0xFF;
	Tx[7] = Mask & 0xFF;
	Tx[8] = (Mask >> 24) & 0xFF;
	Tx[9] = (Mask >> 16) & 0xFF;

	Tx[10] = (Value >> 8) & 0xFF;
	Tx[11] = Value & 0xFF;
	Tx[12] = (Value >> 24) & 0xFF;
	Tx[13] = (Value >> 16) & 0xFF;

	for (int i = 2; i < 14; i++)
		CheckSum += Tx[i];

	Tx[14] = (CheckSum >> 8) & 0xFF;
	Tx[15] = CheckSum & 0xFF;

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

	uint32_t Rv = MtPerformHbppAtnAck();
	LogMsg("MtWriteRegister: Returned 0x%04x", Rv);
	return Rv == 0x4AD1;
}

bool MtAttemptCalibration()
{
	uint32_t Z2Version;
	uint8_t Tx[2], Rx[2];

	Z2Version = MtReadRegister(0x10008FFC);
	LogMsg("zephyr2: Detected firmware version 0x%08x", Z2Version);

	if (Z2Version != 0x5A020028)
	{
		if (!MtWriteRegister(0x10001C04, 0x16E4, 0x1FFF)) {
			LogMsg("zephyr2: Error writing to register 0x10001c04");
			return false;
		}
		if (!MtWriteRegister(0x10001C08, 0x840000, 0xFF0000)) {
			LogMsg("zephyr2: Error writing to register 0x10001c08");
			return false;
		}
		if (!MtWriteRegister(0x1000300C, 0x05, 0x85)) {
			LogMsg("zephyr2: Error writing to register 0x1000300c");
			return false;
		}
		if (!MtWriteRegister(0x1000304C, 0x20, 0xFFFFFFFF)) {
			LogMsg("zephyr2: Error writing to register 0x1000304c");
			return false;
		}
		LogMsg("zephyr2: initialized registers");
	}

	Tx[0] = 0x1F;
	Tx[1] = 0x01;

	LogMsg("zephyr2: Requesting calibration.");

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

	OSSleep(65);

	LogMsg("zephyr2: Calibration complete with 0x%04x", MtPerformHbppAtnAck());
	return true;
}

void MtSendExecutePacket()
{
	uint8_t Tx[12], Rx[12];
	uint32_t CheckSum = 0;

	Tx[0] = 0x1D;
	Tx[1] = 0x53;
	Tx[2] = 0x18;
	Tx[3] = 0x00;
	Tx[4] = 0x10;
	Tx[5] = 0x00;
	Tx[6] = 0x00;
	Tx[7] = 0x01;
	Tx[8] = 0x00;
	Tx[9] = 0x00;

	for (int i = 2; i < 10; i++) {
		CheckSum += Tx[i];
	}

	Tx[10] = (CheckSum >> 8) & 0xFF;
	Tx[11] = CheckSum & 0xFF;

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);
	LogMsg("zephyr2: Execute packet sent");
}

bool MtDetermineInterfaceVersion()
{
	uint8_t Tx[16], Rx[16];
	uint16_t CheckSum = 0;

	memset(Tx, 0, sizeof Tx);

	Tx[0] = 0xE2;

	CheckSum = 0;
	for (int i = 0; i < 14; i++) {
		CheckSum += Tx[i];
	}

	// N.B.  Byte order changes after firmware upload
	Tx[14] = CheckSum & 0xFF;
	Tx[15] = (CheckSum >> 8) & 0xFF;

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

	for (int Try = 0; Try < 4; Try++)
	{
		// @BUG: doing this again!?
		MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

		if (Rx[0] != 0xE2) {
			MtInterfaceVersion = 0;
			MtMaxPacketSize = 1000;
			OSSleep(3);
			continue;
		}

		// verify checksum now
		CheckSum = 0;
		for (int i = 0; i < 14; i++) {
			CheckSum += Rx[i];
		}

		if (CheckSum != (uint16_t)(Rx[14] | (Rx[15] << 8))) {
			LogMsg("zephyr2: Message has checksum %04x where we expected 0x%02x%02x", CheckSum, Rx[15], Rx[14]);
			continue;
		}

		MtInterfaceVersion = Rx[2];
		MtMaxPacketSize = (Rx[4] << 8) | Rx[3];
		return true;
	}

	return false;
}

static int MtGetReportInfo(int id, uint8_t* Error, uint16_t* OutLength)
{
	uint8_t Tx[16];
	uint8_t Rx[16];
	uint16_t CheckSum;

	for (int Try = 0; Try < 4; Try++)
	{
		memset(Tx, 0, sizeof(Tx));

		Tx[0] = 0xE3;
		Tx[1] = id;

		CheckSum = 0;
		for (int i = 0; i < 14; i++)
			CheckSum += Tx[i];

		Tx[14] = CheckSum & 0xFF;
		Tx[15] = (CheckSum >> 8) & 0xFF;

		MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

		OSSleep(1);

		MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

		if (Rx[0] != 0xE3) {
			LogMsg("zephyr2: report info invalid header");
			continue;
		}

		CheckSum = 0;
		for (int i = 0; i < 14; i++)
			CheckSum += Rx[i];

		if ((uint16_t)(Rx[14] | (Rx[15] << 8)) != CheckSum) {
			LogMsg("zephyr2: Report has checksum %04x where we expected 0x%02x%02x", CheckSum, Rx[15], Rx[14]);
			continue;
		}

		*Error = Rx[2];
		*OutLength = (Rx[4] << 8) | Rx[3];

		return true;
	}

	return false;
}

static bool MtShortControlRead(int Id, uint8_t* Buffer, int BufferSize)
{
	uint16_t CheckSum;

	memset(GetInfoPacket, 0, sizeof GetInfoPacket);
	GetInfoPacket[0] = 0xE6;
	GetInfoPacket[1] = Id;
	GetInfoPacket[2] = 0;
	GetInfoPacket[3] = BufferSize & 0xFF;
	GetInfoPacket[4] = (BufferSize >> 8) & 0xFF;

	CheckSum = 0;
	for (int i = 0; i < 5; i++)
		CheckSum += GetInfoPacket[i];

	GetInfoPacket[14] = CheckSum & 0xFF;
	GetInfoPacket[15] = (CheckSum >> 8) & 0xFF;

	MtSpiTxRx(&MtNormalSpeed, GetInfoPacket, 16, InputPacket, 16);

	OSSleep(1);

	GetInfoPacket[2] = 1;

	MtSpiTxRx(&MtNormalSpeed, GetInfoPacket, 16, InputPacket, 16);

	CheckSum = 0;
	for (int i = 0; i < 14; i++)
		CheckSum += InputPacket[i];

	if ((uint16_t) (InputPacket[14] | (InputPacket[15] << 8)) != CheckSum)
		return false;

	memcpy(Buffer, &InputPacket[3], BufferSize);

	return true;
}

static bool MtLongControlRead(int Id, uint8_t* Buffer, int BufferSize)
{
	uint16_t CheckSum;

	memset(GetInfoPacket, 0, sizeof GetInfoPacket);
	GetInfoPacket[0] = 0xE7;
	GetInfoPacket[1] = Id;
	GetInfoPacket[2] = 0;
	GetInfoPacket[3] = BufferSize & 0xFF;
	GetInfoPacket[4] = (BufferSize >> 8) & 0xFF;

	CheckSum = 0;
	for (int i = 0; i < 5; i++)
		CheckSum += GetInfoPacket[i];

	GetInfoPacket[14] = CheckSum & 0xFF;
	GetInfoPacket[15] = (CheckSum >> 8) & 0xFF;

	MtSpiTxRx(&MtNormalSpeed, GetInfoPacket, 16, InputPacket, 16);

	OSSleep(1);

	GetInfoPacket[2] = 1;
	GetInfoPacket[14] = 0;
	GetInfoPacket[15] = 0;
	GetInfoPacket[3 + BufferSize] = CheckSum & 0xFF;
	GetInfoPacket[3 + BufferSize + 1] = (CheckSum >> 8) & 0xFF;

	MtSpiTxRx(&MtNormalSpeed, GetInfoPacket, BufferSize + 5, InputPacket, BufferSize + 5);

	CheckSum = 0;
	for (int i = 0; i < BufferSize + 3; i++)
		CheckSum += InputPacket[i];

	if ((uint16_t)(InputPacket[3 + BufferSize] | (InputPacket[3 + BufferSize + 1] << 8)) != CheckSum)
		return false;

	memcpy(Buffer, &InputPacket[3], BufferSize);

	return true;
}

bool MtGetReport(int Id, void* Buffer, size_t* OutLength)
{
	uint8_t Error;
	uint16_t Length;
	if (!MtGetReportInfo(Id, &Error, &Length))
		return false;

	if (Error)
		return false;

	*OutLength = Length;

	for (int Try = 0; Try < 4; Try++)
	{
		if (Length < 12)
		{
			if (MtShortControlRead(Id, Buffer, Length))
				return true;
		}
		else
		{
			if (MtLongControlRead(Id, Buffer, Length))
				return true;
		}
	}

	return false;
}

void MtSetupMultitouchPart2()
{
	bool FirmwareSucceeded = false;
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
			FirmwareSucceeded = true;
			break;
		}
	}

	if (!FirmwareSucceeded)
	{
		LogMsg("zephyr2: Failed to upload firmware.");

		if (!MtCalBuffer) {
			LogMsg("zephyr2: Bypassing setup procedure. (you might be running on an emulator)");
		}
		else {
			LogMsg("zephyr2: Multitouch will be inoperable for this session.");
		}
		return;
	}

	if (MtModel != N45AP)
	{
		if (!MtAttemptUploadCalibrationData(ProxCalBuffer, ProxCalLength, PROX_CALIBRATION_ADDRESS))
		{
			LogMsg("zephyr2: Failed to upload prox calibration data.");
			return;
		}

		LogMsg("zephyr2: Prox clibration data was uploaded successfully.");
	}

	if (!MtAttemptUploadCalibrationData(MtCalBuffer, MtCalLength, MT_CALIBRATION_ADDRESS))
	{
		LogMsg("zephyr2: Failed to upload calibration data.");
		return;
	}

	LogMsg("zephyr2: Calibration data was uploaded successfully.");

	if (!MtAttemptCalibration())
	{
		LogMsg("zephyr2: Failed to calibrate.");
		return;
	}

	LogMsg("zephyr2: Calibration successful.");

	MtSendExecutePacket();

	OSSleep(1);

	LogMsg("zephyr2: Determining interface version...");

	if (!MtDetermineInterfaceVersion())
	{
		LogMsg("zephyr2: Failed to determine interface version.");
		return;
	}

	LogMsg("zephyr2: Interface version %d, max packet size %d", MtInterfaceVersion, MtMaxPacketSize);

	MtFamilyIdBuffer = MmAllocatePool(POOL_NONPAGED, MtMaxPacketSize);
	MtSensorInfoBuffer = MmAllocatePool(POOL_NONPAGED, MtMaxPacketSize);
	MtSensorRegionDescBuffer = MmAllocatePool(POOL_NONPAGED, MtMaxPacketSize);
	MtSensorRegionParamBuffer = MmAllocatePool(POOL_NONPAGED, MtMaxPacketSize);
	MtSensorDimBuffer = MmAllocatePool(POOL_NONPAGED, MtMaxPacketSize);

	if (!MtFamilyIdBuffer ||
		!MtSensorInfoBuffer ||
		!MtSensorRegionDescBuffer ||
		!MtSensorRegionParamBuffer ||
		!MtSensorDimBuffer)
	{
		// TODO: don't crash here.
		KeCrash("zephyr2: Cannot allocate required buffers");
	}

	if (!MtGetReport(MT_INFO_FAMILY_ID, MtFamilyIdBuffer, &MtFamilyIdLength)) {
		LogMsg("zephyr2: Cannot read family ID");
		return;
	}
	if (!MtGetReport(MT_INFO_SENSOR_INFO, MtSensorInfoBuffer, &MtSensorInfoLength)) {
		LogMsg("zephyr2: Cannot read sensor info");
		return;
	}
	if (!MtGetReport(MT_INFO_SENSOR_REGION_DESC, MtSensorRegionDescBuffer, &MtSensorRegionDescLength)) {
		LogMsg("zephyr2: Cannot read sensor region desc");
		return;
	}
	if (!MtGetReport(MT_INFO_SENSOR_REGION_PARAM, MtSensorRegionParamBuffer, &MtSensorRegionParamLength)) {
		LogMsg("zephyr2: Cannot read sensor region param");
		return;
	}
	if (!MtGetReport(MT_INFO_SENSOR_DIM, MtSensorDimBuffer, &MtSensorDimLength)) {
		LogMsg("zephyr2: Cannot read sensor dim");
		return;
	}

	PZ2_FAMILY_ID FamilyId = MtFamilyIdBuffer;
	PZ2_SENSOR_INFO SensorInfo = MtSensorInfoBuffer;
	PZ2_SENSOR_DIM SensorDim = MtSensorDimBuffer;

	int SensorWidth = (9000 - SensorDim->SensorWidthRaw) * 84 / 73;
	int SensorHeight = (13850 - SensorDim->SensorHeightRaw) * 84 / 73;
	uint32_t BcdVersion = (SensorInfo->BcdVersionUpper << 8) | SensorInfo->BcdVersionLower;

	LogMsg("Family ID:     0x%x", FamilyId->FamilyId);
	LogMsg("Sensor rows:   0x%x", SensorInfo->SensorRows);
	LogMsg("Sensor cols:   0x%x", SensorInfo->SensorColumns);
	LogMsg("Sensor width:  0x%x (raw: 0x%x)", SensorWidth, SensorDim->SensorWidthRaw);
	LogMsg("Sensor height: 0x%x (raw: 0x%x)", SensorHeight, SensorDim->SensorHeightRaw);
	LogMsg("BCD Version:   0x%x", BcdVersion);
	LogMsg("Endianness:    0x%x", SensorInfo->Endianness);

	uint8_t* SensorRegionDescU8 = MtSensorRegionDescBuffer;
	uint8_t* SensorRegionParamU8 = MtSensorRegionParamBuffer;

	char PrintBuffer[16];
	LogMsg("Sensor region desc (%zu):", MtSensorRegionDescLength);
	for (size_t i = 0; i < MtSensorRegionDescLength; i++) {
		snprintf(PrintBuffer, sizeof PrintBuffer, "%02x ", SensorRegionDescU8[i]);
		HalDisplayString(PrintBuffer);
	}
	LogMsg("");

	LogMsg("Sensor region param (%zu):", MtSensorRegionParamLength);
	for (size_t i = 0; i < MtSensorRegionParamLength; i++) {
		snprintf(PrintBuffer, sizeof PrintBuffer, "%02x ", SensorRegionParamU8[i]);
		HalDisplayString(PrintBuffer);
	}
	LogMsg("");

	MtFlipNop = false;
	if (BcdVersion > 0x23) {
		MtFlipNop = true;
	}

	MtCurNop = 1;

	LogMsg("zephyr2: Initialized successfully.");
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
