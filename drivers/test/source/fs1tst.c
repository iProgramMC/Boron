/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	fs1tst.c
	
Abstract:
	This module implements the FS1 test for the test driver.
	It tests the functionality of a file system driver (for
	a start, Ext2), such as reading files, and listing the
	root directory.
	
Author:
	iProgramInCpp - 22 June 2025
***/
#include <mm.h>
#include <io.h>
#include <ex.h>
#include <cc.h>
#include <string.h>
#include "utils.h"

const char* RootDir = "\\Mount\\Nvme0Disk1Part0";
const char* SomeFile = "\\Mount\\Nvme0Disk1Part0\\gamma64.nse";

void PerformFs1Test()
{
	PerformDelay(1000, NULL);
	
	HANDLE Handle;
	BSTATUS Status;
	
	Status = ObOpenObjectByName(RootDir, HANDLE_NONE, 0, NULL, &Handle);
	if (FAILED(Status))
		KeCrash("Fs1: Failed to open object %s: %d (%s)", RootDir, Status, RtlGetStatusString(Status));
	
	// Reference this object by its handle.
	void* ObjectV;
	Status = ObReferenceObjectByHandle(Handle, NULL, &ObjectV);
	
	// Check if this object is of the file type.
	if (ObGetObjectType(ObjectV) != IoFileType)
		KeCrash("Fs1: When opened, %s(%p) is not of the type IoFileType.\nObject type: %p, IoFileType: %p", RootDir, ObjectV, ObGetObjectType(ObjectV), IoFileType);
	
	PFILE_OBJECT File = (PFILE_OBJECT) ObjectV;
	
	uint64_t Offset = 0, Version = 0;
	IO_STATUS_BLOCK Iosb;
	IO_DIRECTORY_ENTRY DirEnt;
	while (true)
	{
		Status = IoReadDir(&Iosb, File, Offset, Version, &DirEnt);
		if (Status == STATUS_END_OF_FILE)
			break;
		
		if (FAILED(Status))
			KeCrash("Fs1: Cannot read dir: %d (%s)", Status, RtlGetStatusString(Status));
		
		LogMsg("Inode: %12llu, Type: %8d, Name: %s", DirEnt.InodeNumber, DirEnt.Type, DirEnt.Name);
		Offset  = Iosb.ReadDir.NextOffset;
		Version = Iosb.ReadDir.Version;
	}
	
	ObDereferenceObject(File);
	ObClose(Handle);
	
	LogMsg("Done.");
}
