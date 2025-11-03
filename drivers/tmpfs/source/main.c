/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the main function of the
	Temporary File System driver.
	
Author:
	iProgramInCpp - 1 November 2025
***/
#include "tmpfs.h"

static bool EndsWith(const char* What, const char* With)
{
	size_t WhatLength = strlen(What), WithLength = strlen(With);
	if (WhatLength < WithLength)
		return false;
	
	return strcmp(What + WhatLength - WithLength, With) == 0;
}

IO_DISPATCH_TABLE TmpfsDispatchTable =
{
	//.CreateObject = Ext2CreateObject,
	//.Reference = Ext2ReferenceInode,
	//.Dereference = Ext2DereferenceInode,
	//.Mount = Ext2Mount,
	//.Read = Ext2Read,
	//.ReadDir = Ext2ReadDir,
	//.ParseDir = Ext2ParseDir,
	//.Seekable = Ext2Seekable,
};

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	BSTATUS Status;
	
	bool LoadedAnything = false;
	
	// Scan the loader parameter block for any tar files we should be
	// mounting.
	for (size_t i = 0; i < KeLoaderParameterBlock.ModuleInfo.Count; i++)
	{
		PLOADER_MODULE Module = &KeLoaderParameterBlock.ModuleInfo.List[i];
		
		if (!EndsWith(Module->Path, ".tar"))
			continue;
		
		Status = TmpMountTarFile(Module);
		if (SUCCEEDED(Status))
		{
			LoadedAnything = true;
		}
		else
		{
			LogMsg(
				"Tmpfs: Could not mount tar file %s. %s (%d)",
				Module->Path,
				RtlGetStatusString(Status),
				Status
			);
		}
	}
	
	if (!LoadedAnything)
	{
		LogMsg(
			"Tmpfs.sys did not find any tar files in the boot module list. "
			"Add one or remove this module from the module list."
		);
		return STATUS_UNLOAD;
	}
	
	return STATUS_SUCCESS;
}
