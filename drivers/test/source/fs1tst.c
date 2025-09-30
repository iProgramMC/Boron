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

//const char* RootDir = "/Mount/Nvme0Disk1Part0";
//const char* SomeFile = "/Mount/Nvme0Disk1Part0/gamma64.nse";

void DirectoryList(const char* Path)
{
	LogMsg("Directory of %s\n", Path);
	
	HANDLE Handle;
	BSTATUS Status;
	
	Status = ObOpenObjectByName(Path, HANDLE_NONE, 0, IoFileType, &Handle);
	if (FAILED(Status))
		KeCrash("Fs1: Failed to open %s: %d (%s)", Path, Status, RtlGetStatusString(Status));
	
	// TODO: Replace with OS* calls.
	void* ObjectV;
	Status = ObReferenceObjectByHandle(Handle, NULL, &ObjectV);
	
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
	OSClose(Handle);
}

void FileList(const char* Path, size_t StartOffset, size_t Size, size_t SizePrint)
{
	LogMsg("Reading contents of %s, start offset %zu, size %zu, size printed %zu", Path, StartOffset, Size, SizePrint);
	
	HANDLE Handle;
	BSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	
	OBJECT_ATTRIBUTES Attrs;
	memset(&Attrs, 0, sizeof Attrs);
	Attrs.RootDirectory = HANDLE_NONE;
	Attrs.ObjectName = Path;
	Attrs.ObjectNameLength = strlen(Path);
	Attrs.OpenFlags = 0;
	
	Status = OSOpenFile(&Handle, &Attrs);
	if (FAILED(Status))
		KeCrash("Fs1: Failed to open %s: %d (%s)", Path, Status, RtlGetStatusString(Status));
	
	void* Buffer = MmAllocatePool(POOL_NONPAGED, Size);
	if (!Buffer)
		KeCrash("Fs1: Out of memory, cannot allocate 4K buffer");
	
	Status = OSReadFile(&Iosb, Handle, StartOffset, Buffer, Size, 0);
	if (FAILED(Status))
		KeCrash("Fs1: Failed to read from file: %s (%d)", Status, RtlGetStatusString(Status));
	
	LogMsg("Read in %lld bytes.", Iosb.BytesRead);
	if (Iosb.BytesRead > SizePrint)
		Iosb.BytesRead = SizePrint;
	DumpHex(Buffer, Iosb.BytesRead, true);
	
	LogMsg("Closing now.");
	OSClose(Handle);
	MmFreePool(Buffer);
}

void PerformFs1Test()
{
	PerformDelay(1000, NULL);
	
	DirectoryList("/Mount/Nvme0Disk1Part0");
	FileList("/Mount/Nvme0Disk1Part0/gamma64.nse", 4096, 4096, 512);
	
	LogMsg("** Starting additional tests in 4 seconds...");
	PerformDelay(4000, NULL);
	DirectoryList("/Mount/Nvme0Disk1Part0/d");
	FileList("/Mount/Nvme0Disk1Part0/gamma64.nse", 15681176 - 24, 4096, 512);
	
	LogMsg("** Starting additional tests in 4 seconds...");
	PerformDelay(4000, NULL);
	DirectoryList("/Mount/Nvme0Disk1Part0/ns/initrd/Game");
	FileList("/Mount/Nvme0Disk1Part0/ns/initrd/Game/celeste.nes", 0xCB30, 4096, 512);
}
