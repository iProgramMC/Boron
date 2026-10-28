/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	spi.c

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 24 October 2026
***/
#include <string.h>
#include <hal.h>
#include "nor.h"
#include "pins8900.h"
#include "pins8720.h"

#define NOR_CMD_READ 0x03 // read data
#define NOR_CMD_WREN 0x06 // write enable
#define NOR_CMD_WRDI 0x04 // write disable
#define NOR_CMD_PRGM 0x02 // program mode - write individual bytes
#define NOR_CMD_RDSR 0x05 // read status reg
#define NOR_CMD_WRSR 0x01 // write status reg
#define NOR_CMD_EWSR 0x50 // enable write status reg
#define NOR_CMD_AIPG 0xAD // auto-increment write address
#define NOR_CMD_ERAS 0x20 // erase 4kb
#define NOR_CMD_JDID 0x9F // get JEDEC device info

#define NOR_SR_BUSY (1 << 0) // write in progress
#define NOR_SR_WEL  (1 << 1) // write enabled
#define NOR_SR_BP0  (1 << 2) // block protect 0
#define NOR_SR_BP1  (1 << 3) // block protect 1
#define NOR_SR_BP2  (1 << 4) // block protect 2
#define NOR_SR_BP3  (1 << 5) // block protect 3
#define NOR_SR_AAI  (1 << 6) // auto-increment write address
#define NOR_SR_BPL  (1 << 7) // block protect write disable
#define NOR_SR_BPAL (NOR_SR_BP0 | NOR_SR_BP1 | NOR_SR_BP2 | NOR_SR_BP3)

#define NOR_T_BP 10000
#define NOR_T_SE 25000

FAST_MUTEX NorMutex;

static void NorPrepare();
static void NorFinish();

static int NorSpiTx(const void* TxBuffer, size_t TxBufferSize)
{
	LogMsg("NorSpiTx %zu bytes", TxBufferSize);
	HalSetOutputPinGpio(NorSpiCsPin(), 0);
	LogMsg("Waiting...");
	int Ret = HalTransmitSpi(NorSpiPort(), TxBuffer, TxBufferSize, true, false);
	LogMsg("Done1");
	HalSetOutputPinGpio(NorSpiCsPin(), 1);
	LogMsg("Done2");
	return Ret;
}

static int NorSpiTxRx(const void* TxBuffer, size_t TxBufferSize, void* RxBuffer, size_t RxBufferSize)
{
	HalSetOutputPinGpio(NorSpiCsPin(), 0);
	int Ret = HalTransmitSpi(NorSpiPort(), TxBuffer, TxBufferSize, true, false);
	if (Ret >= 0) {
		Ret = HalReceiveSpi(NorSpiPort(), RxBuffer, RxBufferSize, true, false);
	}
	HalSetOutputPinGpio(NorSpiCsPin(), 1);
	return Ret;
}

static uint8_t NorGetStatus()
{
	uint8_t Data[1], Command[1];
	Command[0] = NOR_CMD_RDSR;
	Data[0] = 0;
	NorSpiTxRx(Command, 1, Data, 1);
	return Data[0];
}

static bool NorWaitForReady(int TimeoutMS)
{
	uint64_t StartTime, EndTime;
	bool TimeoutEnabled = TimeoutMS != TIMEOUT_INFINITE;

	if (TimeoutEnabled) {
		StartTime = HalGetTickCount();
		EndTime = StartTime + TimeoutMS * HalGetTickFrequency() / 1000;
	}

	if (~NorGetStatus() & NOR_SR_BUSY)
		return true;

	while (!TimeoutEnabled || StartTime < EndTime)
	{
		if (~NorGetStatus() & NOR_SR_BUSY)
			return true;

		if (TimeoutEnabled)
		StartTime = HalGetTickCount();
	}

	DbgPrint("nor: Timed out waiting for ready.");
	return false;
}

static void NorWriteDisable()
{
	// Disable write, the NOR driver will not implement support for it.
	uint8_t Command[1];
	Command[0] = NOR_CMD_WRDI;
	NorSpiTx(Command, 1);
}

// Returns the # of bytes read.
static size_t NorSpiRead2(uint8_t* Buffer, size_t BufferSize, uintptr_t Offset)
{
	NorPrepare();

	if (!NorWaitForReady(1000)) {
		NorFinish();
		LogMsg("nor: Could not do NorRead, timed out");
		return 0;
	}

	size_t BytesRead = 0;
	while (BytesRead < BufferSize)
	{
		size_t BytesToRead = BufferSize;
		if (BytesToRead >= NorMaxReadBytes())
			BytesToRead = NorMaxReadBytes();

		uint8_t Data[16];
		uint8_t Command[4];
		ASSERT(NorMaxReadBytes() <= sizeof Data);

		Command[0] = NOR_CMD_READ;
		Command[1] = (Offset >> 16) & 0xFF;
		Command[2] = (Offset >> 8) & 0xFF;
		Command[3] = Offset & 0xFF;

		int Ret = NorSpiTxRx(Command, sizeof Command, Data, NorMaxReadBytes());
		if (Ret < 0) {
			LogMsg("nor: Could not do NorRead, NorSpiTxRx failed");
			break;
		}

		memcpy(Buffer, Data, BytesToRead);
		BytesRead += BytesToRead;
		Buffer += BytesToRead;
		Offset += BytesToRead;
	}

	NorFinish();
	return BytesRead;
}

static void NorSetupChipInfo()
{
	uint8_t Command[1], DeviceId[3];
	memset(DeviceId, 0, sizeof DeviceId);
	Command[0] = NOR_CMD_JDID;

	NorSpiTxRx(Command, 1, DeviceId, 3);

	uint16_t Vendor, Device;
	Vendor = DeviceId[0];
	Device = DeviceId[1] | (DeviceId[2] << 8);

	LogMsg("nor: Vendor %02x, Device %04x", Vendor, Device);

	// For writing, you'd detect the method of writing by the chip vendor.
	// We won't support writing to the NOR, so this is all we need to do.
}

static void NorPrepare()
{
	ExAcquireFastMutex(&NorMutex, false);
	HalSetBaudRateSpi(NorSpiPort(), 12000000, 8, true, false, false);
}

static void NorFinish()
{
	ExReleaseFastMutex(&NorMutex);
}

BSTATUS NorSpiRead(
	PIO_STATUS_BLOCK Iosb,
	UNUSED PFCB Fcb,
	UNUSED uint64_t Offset,
	PMDL MdlBuffer,
	UNUSED uint32_t Flags
)
{
	size_t SizeToRead = MdlBuffer->ByteCount;

	if (SizeToRead == 0)
	{
		Iosb->Status = STATUS_SUCCESS;
		Iosb->BytesRead = 0;
		return STATUS_SUCCESS;
	}

	void* Buffer = NULL;
	BSTATUS Status;
	Status = MmMapPinnedPagesMdl(MdlBuffer, &Buffer);
	if (FAILED(Status))
	{
		Iosb->Status = Status;
		return Status;
	}

	DbgPrint("Sending read request");
	size_t Read = NorSpiRead2(Buffer, MdlBuffer->ByteCount, (uintptr_t) Offset);
	Status = Read == 0 ? STATUS_HARDWARE_IO_ERROR : STATUS_SUCCESS;

	MmUnmapPagesMdl(MdlBuffer);

	Iosb->Status = Status;
	Iosb->BytesRead = Read;

	return Status;
}

bool NorSpiIsSeekable(UNUSED PFCB Fcb)
{
	return true;
}

IO_DISPATCH_TABLE NorSpiDispatchTable =
{
	.Read = NorSpiRead,
	.Seekable = NorSpiIsSeekable,
	// Write will not be supported.
};

bool NorSpiInit()
{
	LogMsg("Initializing SPI NOR...");
	ExInitializeFastMutex(&NorMutex);
	LogMsg("Write disable...");
	NorWriteDisable();
	LogMsg("Setup chip info...");
	NorSetupChipInfo();
	LogMsg("Done...");
	return true;
}

int NorSpiCsPin()
{
	switch (NorModel)
	{
		default:
		case N82AP: return GPIO_SPI0_CS0_N82;
		case N72AP: return GPIO_SPI1_CS0_N72;
		case N88AP: return 0x1204; // TODO
		case K48AP: return 0x505;  // TODO
	}
}

int NorSpiPort()
{
	switch (NorModel)
	{
		default:
		case N88AP:
		case K48AP:
		case N82AP: return 0;
		case N72AP: return 1;
	}
}

size_t NorMaxReadBytes()
{
	switch (NorModel)
	{
		default:
		case N82AP: return 16;
		case N72AP: return 16;
		case N88AP:
		case K48AP: return 4;
	}
}
