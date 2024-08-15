/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/driver.c
	
Abstract:
	This module implements the I/O Driver object type.
	
Author:
	iProgramInCpp - 16 March 2024
***/
#include "iop.h"

INIT
bool IopInitializeDriversDir()
{
	BSTATUS Status = ObCreateDirectoryObject(
		&IoDriversDir,
		NULL,
		"\\Drivers",
		OB_FLAG_KERNEL | OB_FLAG_PERMANENT
	);
	
	if (FAILED(Status))
	{
		DbgPrint("IO: Failed to create \\Drivers directory");
		return false;
	}
	
	// The drivers directory is now created.
	// N.B.  We keep a permanent reference to it at all times, will be useful.
	
	return true;
}

void IopDeleteDriver(void* Object)
{
	// TODO
	DbgPrint("UNIMPLEMENTED: IopDeleteDriver(%p)", Object);
}
