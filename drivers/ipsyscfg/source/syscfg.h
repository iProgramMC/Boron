/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	syscfg.h

Abstract:
	.

Author:
	iProgramInCpp - 25 October 2026
***/
#pragma once

#include <ke.h>
#include <ldr.h>
#include <io.h>
#include <mm.h>
#include <string.h>

#define SYSCFG_OFFSET 0x4000

typedef struct
{
	uint32_t Magic;
	uint32_t BytesUsed;
	uint32_t BytesTotal;
	uint32_t Version;
	uint32_t Unknown;
	uint32_t EntryCount;
}
SYSCFG_HEADER, *PSYSCFG_HEADER;

typedef struct
{
	uint32_t Magic;
	union
	{
		uint8_t Data[16];

		struct
		{
			uint32_t Magic;
			uint32_t Size;
			uint32_t Offset;
		}
		Cntb;
	};
}
SYSCFG_ENTRY, *PSYSCFG_ENTRY;

typedef struct
{
	uint32_t Magic;
	uint32_t Size;
	uint32_t StartOffset;
	LIST_ENTRY ListEntry;

	// Back reference.
	PFCB Fcb;
}
FCB_EXT, *PFCB_EXT;

typedef struct
{
	FAST_MUTEX Mutex;

	// File object that the system config will read from.
	PFILE_OBJECT FileObject;

	// List of FCBs.
	LIST_ENTRY FcbList;

	// Header.
	SYSCFG_HEADER Header;

	// List of entries.
	PSYSCFG_ENTRY Entries;
}
DEVICE_EXT, *PDEVICE_EXT;

#define EXT(Fcb) ((PFCB_EXT)(Fcb->Extension))
#define DEXT(Dev) ((PDEVICE_EXT)(Dev->Extension))

extern PDRIVER_OBJECT gDriverObject;
extern PDEVICE_OBJECT gDeviceObject;
extern IO_DISPATCH_TABLE DispatchTable;

BSTATUS SyscfgRead(
	PIO_STATUS_BLOCK Iosb,
	PFCB Fcb,
	uint64_t Offset,
	PMDL MdlBuffer,
	uint32_t Flags
);

BSTATUS SyscfgReadDir(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	uint64_t Offset,
	uint64_t Version,
	PIO_DIRECTORY_ENTRY DirectoryEntry
);

BSTATUS SyscfgParseDir(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	const char* ParsePath,
	int ParseLoopCount,
	PIO_PARSE_CONTEXT ParseContext
);

void SyscfgReferenceFcb(
	PFCB Fcb
);

void SyscfgDereferenceFcb(
	PFCB Fcb
);

bool SyscfgSeekableFcb(
	PFCB Fcb
);

BSTATUS SyscfgOpenNorIfNeeded();
