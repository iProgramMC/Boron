/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	cfi.c

Abstract:
	.

Author:
	iProgramInCpp - 24 October 2026
***/
#include "nor.h"

#define NOR_BASE 0x24000000

#define NOR_COMMAND 0xAAAA
#define LOCK 0x5554
#define VENDOR 0
#define DEVICE 2

#define DATA_MODE 0xFFFF
#define COMMAND_UNLOCK 0xAAAA
#define COMMAND_LOCK 0xF0F0
#define COMMAND_IDENTIFY 0x9090
#define COMMAND_WRITE 0xA0A0
#define COMMAND_ERASE 0x8080
#define ERASE_DATA 0x3030
#define LOCK_UNLOCK 0x5555

void* NorCfiBase;

static uint16_t NorVendor, NorDevice;

bool NorCfiIsSeekable(UNUSED PFCB Fcb)
{
	return true;
}

BSTATUS NorCfiRead(
	PIO_STATUS_BLOCK Iosb,
	UNUSED PFCB Fcb,
	UNUSED uint64_t Offset,
	PMDL MdlBuffer,
	UNUSED uint32_t Flags
)
{
	void* Buffer = NULL;
	BSTATUS Status;
	Status = MmMapPinnedPagesMdl(MdlBuffer, &Buffer);
	if (FAILED(Status))
	{
		Iosb->Status = Status;
		return Status;
	}

	uint8_t* Buffer8 = Buffer;
	uint16_t* Buffer16 = Buffer;

	for (size_t i = 0; i < MdlBuffer->ByteCount; i += 2)
	{
		volatile uint16_t* P = &REG16(NorCfiBase, (uint32_t) Offset);
		Buffer16[i >> 1] = *P;
		Offset += 2;
	}

	if (MdlBuffer->ByteCount & 1)
	{
		Buffer8[MdlBuffer->ByteCount - 1] = (uint8_t)(REG16(NorCfiBase, (uint32_t) Offset) & 0xFF);
	}

	MmUnmapPagesMdl(MdlBuffer);

	Iosb->Status = STATUS_SUCCESS;
	Iosb->BytesRead = MdlBuffer->ByteCount;

	return STATUS_SUCCESS;
}

IO_DISPATCH_TABLE NorCfiDispatchTable =
{
	.Read = NorCfiRead,
	.Seekable = NorCfiIsSeekable,

	// TODO: Write support.
};

bool NorCfiInit()
{
	NorCfiBase = MmMapIoSpace(
		NOR_BASE,
		NorSize(),
		MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
		POOL_TAG("NorF")
	);

	if (!NorCfiBase)
	{
		LogMsg("NOR: Cannot map CFI base.");
		return false;
	}

	UNUSED uint16_t Dummy;

	void* Base = NorCfiBase;
	REG16(Base, NOR_COMMAND) = COMMAND_UNLOCK;
	REG16(Base, LOCK) = LOCK_UNLOCK;

	REG16(Base, NOR_COMMAND) = COMMAND_IDENTIFY;
	Dummy = REG16(Base, 0);
	Dummy = REG16(Base, 0);

	NorVendor = REG16(Base, VENDOR);
	NorDevice = REG16(Base, DEVICE);

	REG16(Base, NOR_COMMAND) = COMMAND_LOCK;
	REG16(Base, 0) = DATA_MODE;
	Dummy = REG16(Base, 0);
	Dummy = REG16(Base, 0);

	if (!NorVendor) {
		LogMsg("NOR not found.");
		return false;
	}

	LogMsg("NOR vendor: %04x, device: %04x", NorVendor, NorDevice);
	return true;
}
