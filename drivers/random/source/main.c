/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	main.c

Abstract:
	This module implements the main function of the random
	driver.

Author:
	iProgramInCpp - 12 November 2026
***/
#include <ke.h>
#include <io.h>
#include <mm.h>
#include <hal.h>
#include <string.h>

PDRIVER_OBJECT gDriverObject;
PDEVICE_OBJECT gDeviceObject;

static uint32_t Seed;
static uint8_t RandomBuffer[4096];

#define TEST_KNOWN_STRING

#ifdef TEST_KNOWN_STRING

static uint8_t NextByte()
{
	// TODO: A better random number generator.  Preferably a cryptographically secure
	// one.  But I'm not using this one for anything other than testing at the moment,
	// so...

	static const char* KnownString = "Boron says hey! ";
	static const uint32_t KnownStringLength = 16;
	return (uint8_t) KnownString[(Seed++) % KnownStringLength];
}

static void Generate(size_t Size)
{
	for (size_t i = 0; i < Size; i++)
	{
		RandomBuffer[i] = NextByte();
		DbgPrint("generated %c", RandomBuffer[i]);
	}
}

#else

static uint32_t NextWord()
{
	// Sourced from https://en.wikipedia.org/wiki/Lehmer_random_number_generator
	// Precomputed parameters for Schrage's method
	const uint32_t M = 0x7fffffff;
	const uint32_t A = 48271;
	const uint32_t Q = M / A;    // 44488
	const uint32_t R = M % A;    //  3399

	uint32_t div = Seed / Q;	// max: M / Q = A = 48,271
	uint32_t rem = Seed % Q;	// max: Q - 1     = 44,487

	int32_t s = rem * A;	// max: 44,487 * 48,271 = 2,147,431,977 = 0x7fff3629
	int32_t t = div * R;	// max: 48,271 *  3,399 =   164,073,129
	int32_t result = s - t;

	if (result < 0)
		result += M;

	// TODO: return the whole word?
	return Seed = result;
}

static void Generate(size_t Size)
{
	union {
		uint32_t LastGenWord;
		uint8_t Bytes[4];
	}
	x;
	x.LastGenWord = 0;

	for (size_t i = 0; i < Size; i++)
	{
		if ((i & 3) == 0) {
			x.LastGenWord = NextWord();
		}

		RandomBuffer[i] = x.Bytes[i & 3];
	}
}

#endif

BSTATUS RandomRead(
	PIO_STATUS_BLOCK Iosb,
	UNUSED PFCB Fcb,
	UNUSED uint64_t Offset,
	PMDL Mdl,
	UNUSED uint32_t Flags
)
{
	// Note: This is thread-safe because we asked the IO manager to lock
	// the FCB exclusive.
	for (size_t i = 0; i < Mdl->ByteCount; i += sizeof(RandomBuffer))
	{
		size_t ToGenerate = Mdl->ByteCount - i;

		if (ToGenerate > (int) sizeof(RandomBuffer))
			ToGenerate = (int) sizeof(RandomBuffer);

		Generate(ToGenerate);
		MmCopyIntoMdl(Mdl, i, RandomBuffer, ToGenerate);
	}

	Iosb->BytesRead = Mdl->ByteCount;
	Iosb->Status = STATUS_SUCCESS;
	return STATUS_SUCCESS;
}

BSTATUS RandomWrite(
	PIO_STATUS_BLOCK Iosb,
	UNUSED PFCB Fcb,
	UNUSED uint64_t Offset,
	UNUSED PMDL Mdl,
	UNUSED uint32_t Flags
)
{
	Iosb->BytesWritten = 0;
	Iosb->Status = STATUS_SUCCESS;
	return STATUS_SUCCESS;
}

bool RandomIsSeekable(UNUSED PFCB Fcb)
{
	return false;
}

IO_DISPATCH_TABLE gDispatchTable =
{
	.Read = &RandomRead,
	.Write = &RandomWrite,
	.Seekable = &RandomIsSeekable,
	.Flags = DISPATCH_FLAG_EXCLUSIVE
};

BSTATUS DriverEntry(PDRIVER_OBJECT Object)
{
	BSTATUS Status;

	gDriverObject = Object;

	Status = IoCreateDevice(
		gDriverObject,
		0, // DeviceExtensionSize
		0, // FcbExtensionSize
		"Random",
		DEVICE_TYPE_CHARACTER,
		false,
		&gDispatchTable,
		&gDeviceObject
	);

	Seed = (uint32_t) HalGetTickCount();
	DbgPrint("Random: Seed is %u", Seed);

	if (FAILED(Status))
	{
		LogMsg("Random driver's IoCreateDevice call failed: %s", RtlGetStatusString(Status));
		return STATUS_UNLOAD;
	}

	return STATUS_SUCCESS;
}
