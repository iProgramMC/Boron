/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	init.c

Abstract:
	This module implements the initialization procedure for
	the zephyr1 multitouch driver.

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

const char* MtASpeedFilePath = "/Root/etc/zephyr_aspeed.bin";
const char* MtFilePath = "/Root/etc/zephyr_main.bin";

static uint8_t OutputPacket[1024];
static uint8_t InputPacket[1024];
static uint8_t GetInfoPacket[1024];

void *FirmwareBuffer, *ASpeedFirmwareBuffer;
size_t FirmwareLength, ASpeedFirmwareLength;

void *MtFamilyIdBuffer, *MtSensorInfoBuffer, *MtSensorRegionDescBuffer, *MtSensorRegionParamBuffer, *MtSensorDimBuffer;
size_t MtFamilyIdLength, MtSensorInfoLength, MtSensorRegionDescLength, MtSensorRegionParamLength, MtSensorDimLength;

static void MakeBootloaderDataPacket(uint8_t* OutputBuffer, uint32_t TargetAddress, const uint8_t* Data, size_t DataLen, int* CheckSumOut)
{
	uint32_t CheckSum;

	OutputBuffer[0] = 0xC2;
	OutputBuffer[1] = (TargetAddress >> 24) & 0xFF;
	OutputBuffer[2] = (TargetAddress >> 16) & 0xFF;
	OutputBuffer[3] = (TargetAddress >> 8) & 0xFF;
	OutputBuffer[4] = TargetAddress & 0xFF;
	OutputBuffer[5] = 0;

	CheckSum = 0;
	for (size_t i = 0; i < DataLen; i++)
	{
		CheckSum += Data[i];
		OutputBuffer[6 + i] = Data[i];
	}

	for (size_t i = 0; i < 6; i++)
	{
		CheckSum += OutputBuffer[i];
	}

	if (DataLen < 0x3F8) {
		memset(OutputBuffer + DataLen + 6, 0, 0x3F8 - DataLen);
	}

	OutputBuffer[0x3FE] = (CheckSum >> 8);
	OutputBuffer[0x3FF] = CheckSum & 0xFF;

	if (CheckSumOut) {
		*CheckSumOut = CheckSum;
	}
}

bool MtVerifyUpload(uint16_t CheckSum)
{
	uint8_t Tx[4], Rx[4];
	Tx[0] = 0x05;
	Tx[1] = 0x00;
	Tx[2] = 0x00;
	Tx[3] = 0x06;
	Rx[0] = Rx[1] = Rx[2] = Rx[3] = 0;

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

	if (Rx[0] != 0xD0 || Rx[1] != 0x00) {
		LogMsg("zephyr1: Data verification failed: type bytes are wrong.  Got: %02x %02x %02x %02x", Rx[0], Rx[1], Rx[2], Rx[3]);
		return false;
	}

	if ((Rx[2] != (CheckSum >> 8)) || (Rx[3] != (CheckSum & 0xFF))) {
		LogMsg("zephyr1: Data verification failed: checksum is wrong.  Got: %02x%02x %04x", Rx[2], Rx[3], CheckSum);
		return false;
	}

	return true;
}

void MtSendBlankDataPacket()
{
	uint8_t Tx[4], Rx[4];

	Tx[0] = 0xC2;
	Tx[1] = 0x00;
	Tx[2] = 0x00;
	Tx[3] = 0x00;

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);
	LogMsg("zephyr1: Blank data packet sent.");
}

void MtSendExecutePacket()
{
	uint8_t Tx[4], Rx[4];

	Tx[0] = 0xC4;
	Tx[1] = 0x00;
	Tx[2] = 0x00;
	Tx[3] = 0xC4;

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);
	LogMsg("zephyr1: Execute packet sent.");
}

bool MtAttemptUploadASpeedFirmware(void* Buffer, size_t Length, uint32_t Address)
{
	const uint8_t* Data = Buffer;
	size_t Left = (Length + 3) & ~0x3;

	while (Left > 0)
	{
		size_t ToUpload = Left;
		if (ToUpload > 0x3F8)
			ToUpload = 0x3F8;

		int CheckSum = 0;
		MakeBootloaderDataPacket(OutputPacket, Address, Data, ToUpload, &CheckSum);

		const int Tries = 5;
		int Try;
		for (Try = 0; Try < Tries; Try++)
		{
			LogMsg("zephyr1: uploading A-Speed firmware (attempt %d/%d) (CS: %04x)", Try + 1, Tries, CheckSum);

			int ReturnValue = MtSpiTx(&MtNormalSpeed, OutputPacket, sizeof(OutputPacket));
			if ((size_t) ReturnValue != sizeof(OutputPacket))
				LogMsg("zephyr1: ERROR: MtSpiTx value is %d, expected %zu", ReturnValue, sizeof(OutputPacket));

			if (MtVerifyUpload(CheckSum))
				break;
		}

		if (Try == 5)
			return false;

		Address += ToUpload;
		Data += ToUpload;
		Left -= ToUpload;
	}

	MtSendExecutePacket();
	return true;
}

bool MtAttemptUploadMainFirmware(uint8_t* Buffer, size_t Length)
{
	uint16_t CheckSum = 0;
	for (size_t i = 0; i < Length; i++)
		CheckSum += Buffer[i];

	for (int i = 0; i < 6; i++)
	{
		if (i == 5)
			return false;

		MtSendBlankDataPacket();

		MtSpiTx(&MtFastSpeed, Buffer, Length);

		if (MtVerifyUpload(CheckSum))
			break;
	}

	MtSendExecutePacket();
	return true;
}

bool MtDetermineInterfaceVersion()
{
	uint8_t Tx[4], Rx[4];
	memset(Tx, 0xD0, sizeof Tx);

	for (int Try = 0; Try < 4; Try++)
	{
		MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

		if (Rx[0] != 0xAA)
		{
			MtInterfaceVersion = 0;
			MtMaxPacketSize = 1000;
			OSSleep(3);
			continue;
		}

		MtInterfaceVersion = Rx[1];
		MtMaxPacketSize = (Rx[2] << 8) | Rx[3];
		return true;
	}

	return false;
}

static int MtGetReportInfo(int Id, uint8_t* Error, uint16_t* OutLength)
{
	uint8_t Tx[8], Rx[8];

	for (int i = 0; i < 8; i++)
	{
		memset(Tx, 0x8F, sizeof Tx);
		Tx[1] = Id;

		MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

		if (Rx[0] != 0xAA) {
			OSSleep(1);
			continue;
		}

		int RecvCheckSum = ((Rx[6] << 8) | Rx[7]);
		int CheckSum = (Id + Rx[4] + Rx[5]) & 0xFFFF;

		if (CheckSum != RecvCheckSum) {
			OSSleep(1);
			continue;
		}

		*Error = (Rx[4] >> 4) & 0xF;
		*OutLength = ((Rx[4] & 0xF) << 8) | Rx[5];
		return true;
	}

	return false;
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
		memset(GetInfoPacket, 0x82, sizeof(GetInfoPacket));
		GetInfoPacket[1] = Id;

		MtSpiTxRx(&MtNormalSpeed, GetInfoPacket, Length + 6, InputPacket, Length + 6);
		if (InputPacket[0] != 0xAA) {
			OSSleep(1);
			continue;
		}

		uint16_t CheckSumRecv = (InputPacket[Length + 4] << 8) | InputPacket[Length + 5];
		uint16_t CheckSum = Id;

		for (size_t i = 0; i < Length; i++) {
			CheckSum += InputPacket[4 + i];
		}

		if (CheckSum != CheckSumRecv) {
			OSSleep(1);
			continue;
		}

		*OutLength = Length;
		memcpy(Buffer, InputPacket + 4, Length);
		return true;
	}

	return false;
}

bool MtSetupMultitouchDevice()
{
	if (!MtAttemptUploadASpeedFirmware(ASpeedFirmwareBuffer, ASpeedFirmwareLength, 0x40000000))
	{
		LogMsg("zephyr1: Failed to upload A-Speed firmware.");
		LogMsg("zephyr1: Multitouch will be inoperable for this session.");
		return false;
	}

	if (!MtAttemptUploadMainFirmware(FirmwareBuffer, FirmwareLength))
	{
		LogMsg("zephyr1: Failed to upload main firmware.");
		LogMsg("zephyr1: Multitouch will be inoperable for this session.");
		return false;
	}

	LogMsg("zephyr1: Determining interface version...");

	if (!MtDetermineInterfaceVersion())
	{
		LogMsg("zephyr1: Failed to determine interface version.");
		return false;
	}

	LogMsg("zephyr1: Interface version %d, max packet size %d", MtInterfaceVersion, MtMaxPacketSize);

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
		KeCrash("zephyr1: Cannot allocate required buffers");
	}

	if (!MtGetReport(MT_INFO_FAMILY_ID, MtFamilyIdBuffer, &MtFamilyIdLength)) {
		LogMsg("zephyr1: Cannot read family ID");
		return false;
	}
	if (!MtGetReport(MT_INFO_SENSOR_INFO, MtSensorInfoBuffer, &MtSensorInfoLength)) {
		LogMsg("zephyr1: Cannot read sensor info");
		return false;
	}
	if (!MtGetReport(MT_INFO_SENSOR_REGION_DESC, MtSensorRegionDescBuffer, &MtSensorRegionDescLength)) {
		LogMsg("zephyr1: Cannot read sensor region desc");
		return false;
	}
	if (!MtGetReport(MT_INFO_SENSOR_REGION_PARAM, MtSensorRegionParamBuffer, &MtSensorRegionParamLength)) {
		LogMsg("zephyr1: Cannot read sensor region param");
		return false;
	}
	if (!MtGetReport(MT_INFO_SENSOR_DIM, MtSensorDimBuffer, &MtSensorDimLength)) {
		LogMsg("zephyr1: Cannot read sensor dim");
		return false;
	}

	PZ2_FAMILY_ID FamilyId = MtFamilyIdBuffer;
	PZ2_SENSOR_INFO SensorInfo = MtSensorInfoBuffer;
	PZ2_SENSOR_DIM SensorDim = MtSensorDimBuffer;

	int SensorWidth = (9000 - SensorDim->SensorWidthRaw) * 84 / 73;
	int SensorHeight = (13850 - SensorDim->SensorHeightRaw) * 84 / 73;
	uint32_t BcdVersion = (SensorInfo->BcdVersionUpper << 8) | SensorInfo->BcdVersionLower;

	MtSensorWidth = SensorWidth;
	MtSensorHeight = SensorHeight;

	LogMsg("zephyr1: Family ID:     0x%x", FamilyId->FamilyId);
	LogMsg("zephyr1: Sensor rows:   0x%x", SensorInfo->SensorRows);
	LogMsg("zephyr1: Sensor cols:   0x%x", SensorInfo->SensorColumns);
	LogMsg("zephyr1: Sensor width:  0x%x (raw: 0x%x)", SensorWidth, SensorDim->SensorWidthRaw);
	LogMsg("zephyr1: Sensor height: 0x%x (raw: 0x%x)", SensorHeight, SensorDim->SensorHeightRaw);
	LogMsg("zephyr1: BCD Version:   0x%x", BcdVersion);
	LogMsg("zephyr1: Endianness:    0x%x", SensorInfo->Endianness);

	uint8_t* SensorRegionDescU8 = MtSensorRegionDescBuffer;
	uint8_t* SensorRegionParamU8 = MtSensorRegionParamBuffer;

	char PrintBuffer[16];
	LogMsg("zephyr1: Sensor region desc (%zu bytes):", MtSensorRegionDescLength);
	for (size_t i = 0; i < MtSensorRegionDescLength; i++) {
		snprintf(PrintBuffer, sizeof PrintBuffer, "%02x ", SensorRegionDescU8[i]);
		HalDisplayString(PrintBuffer);
	}
	LogMsg("");

	LogMsg("zephyr1: Sensor region param (%zu bytes):", MtSensorRegionParamLength);
	for (size_t i = 0; i < MtSensorRegionParamLength; i++) {
		snprintf(PrintBuffer, sizeof PrintBuffer, "%02x ", SensorRegionParamU8[i]);
		HalDisplayString(PrintBuffer);
	}
	LogMsg("");

	MtCurNop = 0x64;

	LogMsg("zephyr1: Initialized successfully.");
	MtInitialized = true;
	return true;
}

bool MtPrepareInitialData()
{
	BSTATUS Status;
	Status = ReadEntireFile(MtFilePath, &FirmwareBuffer, &FirmwareLength, IO_FILE_REGULAR_FILE);
	if (FAILED(Status))
	{
		LogMsg("zephyr1: Could not load %s: %s", MtFilePath, RtlGetStatusString(Status));
		// TODO: Request unload
		return false;
	}

	Status = ReadEntireFile(MtASpeedFilePath, &ASpeedFirmwareBuffer, &ASpeedFirmwareLength, IO_FILE_ANY_TYPE);
	if (FAILED(Status))
	{
		LogMsg("zephyr1: Could not load %s: %s", MtASpeedFilePath, RtlGetStatusString(Status));
		// TODO: Request unload
		return false;
	}

	return true;
}
