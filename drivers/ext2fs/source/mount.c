/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mount.c
	
Abstract:
	This module takes care of the process of mounting
	an Ext2 partition.
	
Author:
	iProgramInCpp - 15 May 2025
***/
#include "ext2.h"

BSTATUS Ext2Mount(PDEVICE_OBJECT BackingDevice, PFILE_OBJECT BackingFile)
{
	// First of all, ensure that this is truly an ext2 file system.
	
	return STATUS_UNIMPLEMENTED;
}

void Ext2DeleteFileSystem(void* FileSystemV)
{
	PEXT2_FILE_SYSTEM FileSystem = FileSystemV;
	
	DbgPrint("Ext2: File System %p Deleted", FileSystem);
	KeCrash("Unimplemented");
}
