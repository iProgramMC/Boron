/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	main.c

Abstract:
	This module implements the initialization procedure for
	the zephyr2 multitouch driver.

Author:
	iProgramInCpp - 27 October 2026
***/
#include "z2.h"

const char* MtFilePath = "/Root/etc/zephyr2.bin";
const char* MtClPath = "/Devices/HardwareConfiguration/MtCl";
const char* PxClPath = "/Devices/HardwareConfiguration/PxCl";

static uint8_t OutputPacket[512];
static uint8_t InputPacket[512];
static uint8_t GetInfoPacket[512];

void *FirmwareBuffer, *ProxCalBuffer, *MtCalBuffer;
size_t FirmwareLength, ProxCalLength, MtCalLength;

void *MtFamilyIdBuffer, *MtSensorInfoBuffer, *MtSensorRegionDescBuffer, *MtSensorRegionParamBuffer, *MtSensorDimBuffer;
size_t MtFamilyIdLength, MtSensorInfoLength, MtSensorRegionDescLength, MtSensorRegionParamLength, MtSensorDimLength;

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
	//LogMsg("zephyr2: Execute packet sent.");
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

static bool MtGetReport(int Id, void* Buffer, size_t* OutLength)
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

void MtSetupMultitouchDevice()
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

		if (MtEmulatorMode) {
			LogMsg("zephyr2: Emulator mode, no need to upload firmware.");
			goto SkipSetupInEmulatorMode;
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
		LogMsg("zephyr2: Multitouch will be inoperable for this session.");
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

SkipSetupInEmulatorMode:
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

	LogMsg("zephyr2: Family ID:     0x%x", FamilyId->FamilyId);
	LogMsg("zephyr2: Sensor rows:   0x%x", SensorInfo->SensorRows);
	LogMsg("zephyr2: Sensor cols:   0x%x", SensorInfo->SensorColumns);
	LogMsg("zephyr2: Sensor width:  0x%x (raw: 0x%x)", SensorWidth, SensorDim->SensorWidthRaw);
	LogMsg("zephyr2: Sensor height: 0x%x (raw: 0x%x)", SensorHeight, SensorDim->SensorHeightRaw);
	LogMsg("zephyr2: BCD Version:   0x%x", BcdVersion);
	LogMsg("zephyr2: Endianness:    0x%x", SensorInfo->Endianness);

	uint8_t* SensorRegionDescU8 = MtSensorRegionDescBuffer;
	uint8_t* SensorRegionParamU8 = MtSensorRegionParamBuffer;

	char PrintBuffer[16];
	LogMsg("zephyr2: Sensor region desc (%zu bytes):", MtSensorRegionDescLength);
	for (size_t i = 0; i < MtSensorRegionDescLength; i++) {
		snprintf(PrintBuffer, sizeof PrintBuffer, "%02x ", SensorRegionDescU8[i]);
		HalDisplayString(PrintBuffer);
	}
	LogMsg("");

	LogMsg("zephyr2: Sensor region param (%zu bytes):", MtSensorRegionParamLength);
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
	MtInitialized = true;
}

NO_RETURN
static void MtInitializationThread(UNUSED void* Context)
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

	Status = ReadEntireFile(MtClPath, &MtCalBuffer, &MtCalLength, IO_FILE_ANY_TYPE);
	if (FAILED(Status))
	{
		MtCalBuffer = NULL;
		MtCalLength = 0;
		LogMsg("MtCl not found. It's possible you're running this on an emulator.");

		// TODO: Actually detect this.  Don't blindly assume that we are.
		MtEmulatorMode = true;
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

	MtSetupMultitouchDevice();

	OSExitThread();
}

bool MtSetupInitializationThread()
{
	PETHREAD Thread;
	BSTATUS Status = PsCreateSystemThreadFast(
		&Thread,
		MtInitializationThread,
		NULL, // StartContext
		false // CreateSuspended
	);

	if (FAILED(Status))
	{
		LogMsg("zephyr2: Cannot create upload thread: %s", RtlGetStatusString(Status));
		return false;
	}

	ObDereferenceObject(Thread);

	return true;
}
