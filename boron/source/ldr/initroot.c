/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ldr/initroot.c
	
Abstract:
	This module implements the function that prepares the
	initial file system root by putting all of the loaded
	kernel modules in an object manager directory.
	
Author:
	iProgramInCpp - 7 April 2025
***/
#include "ldri.h"
#include <ob.h>
#include <io.h>

#define IOSB_STATUS(iosb, stat) (iosb->Status = stat)

typedef struct
{
	PLOADER_MODULE File;
}
INIT_ROOT_FCB_EXT, *PINIT_ROOT_FCB_EXT;

DEVICE_OBJECT LdrpInitRootDevice;

bool LdrInitRootIsSeekable(UNUSED PFCB Fcb)
{
	return true;
}

BSTATUS LdrInitRootRead(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, UNUSED uint32_t Flags)
{
	ASSERT(MdlBuffer->Flags & MDL_FLAG_WRITE);
	
	BSTATUS Status;
	PINIT_ROOT_FCB_EXT Ext = (PINIT_ROOT_FCB_EXT) Fcb->Extension;
	
	size_t Length = MdlBuffer->ByteCount;
	uint64_t OffsetEnd = Offset + Length;
	if (OffsetEnd > Ext->File->Size)
		OffsetEnd = Ext->File->Size;
	
	if (Offset >= OffsetEnd)
	{
		Iosb->BytesRead = 0;
		return IOSB_STATUS(Iosb, STATUS_SUCCESS);
	}
	
	// Map the MDL in.
	void* Buffer = NULL;
	Status = MmMapPinnedPagesMdl(MdlBuffer, &Buffer);
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	// Perform the read.
	memcpy(Buffer, Ext->File->Address + Offset, Length);
	Iosb->BytesRead = OffsetEnd - Offset;
	
	// Clean up after ourselves.
	MmUnmapPagesMdl(MdlBuffer);
	
	return IOSB_STATUS(Iosb, STATUS_SUCCESS);
}

static IO_DISPATCH_TABLE LdrpInitRootDispatchTable =
{
	.Flags = DISPATCH_FLAG_DIRECTLY_OPENABLE,
	.Read = LdrInitRootRead,
	.Seekable = LdrInitRootIsSeekable,
};

POBJECT_DIRECTORY LdriCreateInitialDir()
{
	BSTATUS Status;
	POBJECT_DIRECTORY RootDir;
	
	Status = ObCreateDirectoryObject(&RootDir, NULL, "/InitRoot", OB_FLAG_PERMANENT);
	if (FAILED(Status))
	{
		DbgPrint("Ldr: Failed to create /InitRoot directory (%d)", Status);
		return false;
	}
	
	return RootDir;
}

bool LdriAddFile(POBJECT_DIRECTORY Directory, PLOADER_MODULE File)
{
	// Create the FCB associated with this object.
	PFILE_OBJECT FileObject;
	BSTATUS Status;
	PFCB Fcb;
	PINIT_ROOT_FCB_EXT Ext;
	
	Fcb = IoAllocateFcb(LdrpInitRootDevice.DispatchTable, sizeof(INIT_ROOT_FCB_EXT), true);
	if (!Fcb)
	{
		DbgPrint("Ldr: Could not create FCB for file %s - out of memory", File->Path);
		return false;
	}
	
	// Limine does pass page aligned addresses so this shouldn't be a problem.
	// TODO: This may be a problem with Multiboot.
	ASSERT(((uintptr_t)File->Address & (PAGE_SIZE - 1)) == 0);
	
	Ext = (PINIT_ROOT_FCB_EXT) Fcb->Extension;
	Ext->File = File;
	
	Fcb->FileLength = File->Size;
	Fcb->FileType = FILE_TYPE_FILE;
	
	// Now create the file object.
	Status = ObCreateObject(
		(void**) &FileObject,
		Directory,
		IoFileType,
		File->Path,
		OB_FLAG_PERMANENT,
		NULL,
		sizeof(FILE_OBJECT)
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Ldr: Could not create file object for file %s - error %d", File->Path, Status);
		return false;
	}
	
	// Populate the file object now.
	FileObject->Fcb = Fcb;
	FileObject->Context1 = NULL;
	FileObject->Context2 = NULL;
	FileObject->Flags = 0;
	FileObject->OpenFlags = 0;
	
	return true;
}

bool LdrPrepareInitialRoot()
{
	POBJECT_DIRECTORY RootDir = LdriCreateInitialDir();
	if (!RootDir)
		return false;
	
	// Prepare the device object.
	PDEVICE_OBJECT D = &LdrpInitRootDevice;
	InitializeListHead(&D->ListEntry);
	D->DeviceType = DEVICE_TYPE_BLOCK;
	D->DriverObject = NULL;
	D->DispatchTable = &LdrpInitRootDispatchTable;
	D->Fcb = NULL;
	D->ParentController = NULL;
	D->ParentDevice = NULL;
	D->ExtensionSize = 0;
	
	// Now that the Bin directory is loaded, put everything inside.
	PLOADER_MODULE_INFO ModuleInfo = &KeLoaderParameterBlock.ModuleInfo;
	
	for (uint64_t i = 0; i < ModuleInfo->Count; i++)
	{
		PLOADER_MODULE File = &ModuleInfo->List[i];
		if (!LdriAddFile(RootDir, File))
			return false;
	}
	
#ifdef DEBUG2
	extern POBJECT_DIRECTORY ObpRootDirectory;
	extern BSTATUS ObpDebugDirectory(void* DirP);
	
	DbgPrint("- Init Root setup complete -");
	
	DbgPrint("Dumping root directory:");
	ObpDebugDirectory(ObpRootDirectory);
	DbgPrint("Dumping InitRoot directory:");
	ObpDebugDirectory(RootDir);
#endif
	
	return true;
}


