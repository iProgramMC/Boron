/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	syscfg.c

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 25 October 2026
***/
#include "syscfg.h"

#define CNTB_MAGIC 0x434e5442

// TODO: Hardcoded path.
const char* NorPath = "/Devices/NorFlash";

BSTATUS SyscfgRead(
	PIO_STATUS_BLOCK Iosb,
	PFCB Fcb,
	uint64_t Offset,
	PMDL MdlBuffer,
	uint32_t Flags
)
{
	if (Fcb == gDeviceObject->Fcb)
	{
		DbgPrint("SyscfgRead unsupported on root FCB.");
		return (Iosb->Status = STATUS_UNSUPPORTED_FUNCTION);
	}

	PDEVICE_EXT DeviceExt = DEXT(gDeviceObject);
	BSTATUS Status = SyscfgOpenNorIfNeeded();
	if (FAILED(Status)) {
		DbgPrint("SyscfgRead: Can't open NOR!");
		return (Iosb->Status = Status);
	}

	PFCB_EXT Ext = EXT(Fcb);

	// NOTE: No need to check file bounds.  IoReadFileMdl already takes care of that.
	// (And the caller to this function, too)
	return IoReadFileMdl(
		Iosb,
		DeviceExt->FileObject,
		MdlBuffer,
		Flags,
		Ext->StartOffset + Offset,
		false  // Cached
	);
}

BSTATUS SyscfgReadDir(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	uint64_t Offset,
	uint64_t Version,
	PIO_DIRECTORY_ENTRY DirectoryEntry
)
{
	BSTATUS Status = SyscfgOpenNorIfNeeded();
	if (FAILED(Status))
	{
		DbgPrint("SyscfgRead: Can't open NOR!");
		return (Iosb->Status = Status);
	}

	PDEVICE_EXT DeviceExt = DEXT(gDeviceObject);
	ExAcquireFastMutex(&DeviceExt->Mutex, false);

	if (FileObject->Fcb != gDeviceObject->Fcb) {
		ExReleaseFastMutex(&DeviceExt->Mutex);
		return (Iosb->Status = STATUS_UNSUPPORTED_FUNCTION);
	}

	if (Offset >= DeviceExt->Header.EntryCount) {
		ExReleaseFastMutex(&DeviceExt->Mutex);
		return (Iosb->Status = STATUS_END_OF_FILE);
	}

	(void) Version;

	PSYSCFG_ENTRY Entry = &DeviceExt->Entries[(uint32_t) Offset];

	uint32_t Magic = Entry->Magic;
	if (Magic == CNTB_MAGIC)
		Magic = Entry->Cntb.Magic;

	DirectoryEntry->InodeNumber = Offset;
	DirectoryEntry->Name[0] = Magic >> 24;
	DirectoryEntry->Name[1] = (Magic >> 16) & 0xFF;
	DirectoryEntry->Name[2] = (Magic >> 8) & 0xFF;
	DirectoryEntry->Name[3] = Magic & 0xFF;
	DirectoryEntry->Name[4] = 0;
	DirectoryEntry->Type = FILE_TYPE_FILE;

	Iosb->ReadDir.NextOffset = Offset + 1;
	Iosb->ReadDir.Version = Version;

	ExReleaseFastMutex(&DeviceExt->Mutex);
	return (Iosb->Status = STATUS_SUCCESS);
}

BSTATUS SyscfgParseDir(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	const char* ParsePath,
	int ParseLoopCount,
	PIO_PARSE_CONTEXT ParseContext
)
{
	(void) ParseLoopCount;
	(void) ParseContext;

	BSTATUS Status = SyscfgOpenNorIfNeeded();
	if (FAILED(Status))
	{
		DbgPrint("SyscfgRead: Can't open NOR!");
		return (Iosb->Status = Status);
	}

	PDEVICE_EXT DeviceExt = DEXT(gDeviceObject);
	PFCB InitialFcb = FileObject->Fcb;

	// Is this a normal file?
	if (InitialFcb != gDeviceObject->Fcb)
	{
		if (!ParsePath || *ParsePath == '\0')
		{
		ParsedThis:
			Iosb->ParseDir.FoundFcb = InitialFcb;
			Iosb->ParseDir.ReparsePath = NULL;
			SyscfgReferenceFcb(InitialFcb);
			return (Iosb->Status = STATUS_SUCCESS);
		}

		DbgPrint("Initial Fcb is NOT the device object's FCB, yet remaining ParsePath is '%s'", ParsePath);
		return (Iosb->Status = STATUS_NOT_A_DIRECTORY);
	}

	// Check if we're parsing the directory
	if (*ParsePath == '\0' || *ParsePath == OB_PATH_SEPARATOR)
		goto ParsedThis;

	// Check the path format.
	if (strlen(ParsePath) != 4)
	{
		DbgPrint("Parse path '%s' is invalid!", ParsePath);
		return (Iosb->Status = STATUS_NAME_NOT_FOUND);
	}

	// The lookup magic is the *big-endian* reading of the parse path.
	uint8_t* PathBytes = (uint8_t*) ParsePath;
	uint32_t LookupMagic = (PathBytes[0] << 24) | (PathBytes[1] << 16) | (PathBytes[2] << 8) | PathBytes[3];

	// Not.  Check the FCB list.
	ExAcquireFastMutex(&DeviceExt->Mutex, false);
	for (PLIST_ENTRY Entry = DeviceExt->FcbList.Flink;
		Entry != &DeviceExt->FcbList;
		Entry = Entry->Flink)
	{
		PFCB_EXT Ext = CONTAINING_RECORD(Entry, FCB_EXT, ListEntry);
		PFCB Fcb = Ext->Fcb;

		if (Ext->Magic == LookupMagic)
		{
			// Found it!
			Iosb->ParseDir.FoundFcb = Fcb;
			Iosb->ParseDir.ReparsePath = NULL;
			SyscfgReferenceFcb(Fcb);
			ExReleaseFastMutex(&DeviceExt->Mutex);
			return (Iosb->Status = STATUS_SUCCESS);
		}
	}

	// Not found... have to look through it manually.
	for (uint32_t i = 0; i < DeviceExt->Header.EntryCount; i++)
	{
		PSYSCFG_ENTRY Entry = &DeviceExt->Entries[i];
		bool IsLongEntry = false;
		uint32_t FoundMagic = Entry->Magic;
		if (Entry->Magic == CNTB_MAGIC)
		{
			IsLongEntry = true;
			FoundMagic = Entry->Cntb.Magic;
		}

		if (FoundMagic != LookupMagic)
			continue;

		// Found it!
		PFCB NewFcb = IoAllocateFcb(&DispatchTable, sizeof(FCB_EXT), true);
		if (!NewFcb)
		{
			DbgPrint("Parse path '%s' was found, but we ran out of memory!", ParsePath);
			ExReleaseFastMutex(&DeviceExt->Mutex);
			return (Iosb->Status = STATUS_INSUFFICIENT_MEMORY);
		}

		PFCB_EXT NewExt = EXT(NewFcb);
		NewExt->Fcb   = NewFcb;
		NewExt->Magic = FoundMagic;
		NewExt->Size  = IsLongEntry ? Entry->Cntb.Size : 16;
		NewFcb->FileLength = NewExt->Size;

		if (IsLongEntry)
		{
			NewExt->StartOffset = Entry->Cntb.Offset;
		}
		else
		{
			// TODO: do not require an unnecessary read.
			uint32_t StartOffset = SYSCFG_OFFSET;
			StartOffset += sizeof(SYSCFG_HEADER);
			StartOffset += sizeof(SYSCFG_ENTRY) * i;
			StartOffset += offsetof(SYSCFG_ENTRY, Data);
			NewExt->StartOffset = StartOffset;
		}

		// Append it to the list of loaded FCBs now.
		InsertTailList(&DeviceExt->FcbList, &NewExt->ListEntry);

		// Found it!
		Iosb->ParseDir.FoundFcb = NewFcb;
		Iosb->ParseDir.ReparsePath = NULL;
		SyscfgReferenceFcb(NewFcb);
		ExReleaseFastMutex(&DeviceExt->Mutex);
		return (Iosb->Status = STATUS_SUCCESS);
	}

	DbgPrint("Parse path '%s' was not found!", ParsePath);
	return (Iosb->Status = STATUS_NAME_NOT_FOUND);
}

// This is called on the first read.
BSTATUS SyscfgOpenNorIfNeeded()
{
	BSTATUS Status;
	void* FileObjectV;
	PDEVICE_EXT DeviceExt = DEXT(gDeviceObject);

	ExAcquireFastMutex(&DeviceExt->Mutex, false);

	if (DeviceExt->FileObject)
	{
		// Already opened
		ExReleaseFastMutex(&DeviceExt->Mutex);
		return STATUS_SUCCESS;
	}

	Status = ObReferenceObjectByName(
		NorPath,
		NULL,        // InitialParseObject
		0,           // OpenFlags
		IoFileType,  // ExpectedType
		NULL,        // ParseContext
		&FileObjectV
	);

	if (FAILED(Status))
	{
		LogMsg("Syscfg: Cannot open NOR at %s: %s", NorPath, RtlGetStatusString(Status));
		ExReleaseFastMutex(&DeviceExt->Mutex);
		return Status;
	}

	// NOR opened.  Assign the device extension its pointer now.
	DeviceExt->FileObject = FileObjectV;

	// Read the header.
	IO_STATUS_BLOCK Iosb;
	Status = IoReadFile(
		&Iosb,
		DeviceExt->FileObject,
		&DeviceExt->Header,
		sizeof(DeviceExt->Header),
		0,             // Flags
		SYSCFG_OFFSET, // FileOffset
		false,         // Cached
		true           // TrustBuffer
	);

	if (FAILED(Status))
	{
		DbgPrint("Syscfg: Failed to read header from NOR: %s", RtlGetStatusString(Status));
		ExReleaseFastMutex(&DeviceExt->Mutex);
		return Status;
	}

	// Set the device object's FCB's size to the size reported in the header.
	gDeviceObject->Fcb->FileLength = DeviceExt->Header.BytesTotal;

	DeviceExt->Entries = MmAllocatePool(POOL_NONPAGED, sizeof(SYSCFG_ENTRY) * DeviceExt->Header.EntryCount);
	if (!DeviceExt->Entries)
	{
		DbgPrint("Syscfg: Failed to allocate memory for %u entries: %s", DeviceExt->Header.EntryCount, RtlGetStatusString(Status));
		ExReleaseFastMutex(&DeviceExt->Mutex);
		return Status;
	}

	Status = IoReadFile(
		&Iosb,
		DeviceExt->FileObject,
		DeviceExt->Entries,
		sizeof(SYSCFG_ENTRY) * DeviceExt->Header.EntryCount,
		0,             // Flags
		SYSCFG_OFFSET + sizeof(SYSCFG_HEADER),
		false,         // Cached
		true           // TrustBuffer
	);

	if (FAILED(Status))
	{
		DbgPrint("Syscfg: Failed to read entries from NOR: %s", RtlGetStatusString(Status));
		MmFreePool(DeviceExt->Entries);
		DeviceExt->Entries = NULL;
		ExReleaseFastMutex(&DeviceExt->Mutex);
		return Status;
	}

	ExReleaseFastMutex(&DeviceExt->Mutex);
	return STATUS_SUCCESS;
}

void SyscfgReferenceFcb(
	PFCB Fcb
)
{
	// TODO: reference counting.
	(void) Fcb;
}

void SyscfgDereferenceFcb(
	PFCB Fcb
)
{
	// TODO: reference counting.
	(void) Fcb;
}

bool SyscfgSeekableFcb(
	PFCB Fcb
)
{
	(void) Fcb;
	return true;
}
