/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	cfi.c

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

static uint8_t* CopyMemory16(uint8_t* Dest, volatile uint16_t* Source, size_t ShortCount)
{
	if (!ShortCount)
		return Dest;

	bool IsUnaligned = (uintptr_t) Dest & 1;

	if (LIKELY(!IsUnaligned))
	{
		// Aligned case
		for (size_t i = 0; i < ShortCount; i++)
		{
			*(uint16_t*)Dest = Source[i];
			Dest += 2;
		}
	}
	else
	{
		// Unaligned case
		for (size_t i = 0; i < ShortCount; i++)
		{
			uint16_t Data = Source[i];
			Dest[0] = Data & 0xFF;
			Dest[1] = Data >> 8;
			Dest += 2;
		}
	}

	return Dest;
}

BSTATUS NorCfiRead(
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

	// Read first byte if unaligned
	uint8_t* Buffer8 = Buffer;
	if (Offset & 1)
	{
		uint16_t CompleteShort = REG16(NorCfiBase, (uint32_t)(Offset & ~1));
		Buffer8[0] = CompleteShort >> 8;
		Buffer8++;
		Offset++;
		SizeToRead--;
	}

	// Read bytes in the middle
	Buffer8 = CopyMemory16(Buffer8, &REG16(NorCfiBase, (uint32_t) Offset), SizeToRead >> 1);

	// If there is one more byte to read, read that too
	if (SizeToRead & 1)
	{
		uint16_t CompleteShort = REG16(NorCfiBase, (uint32_t)(Offset & ~1));
		Buffer8[0] = CompleteShort & 0xFF;
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
	// Write will not be supported.
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
