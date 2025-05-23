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

BSTATUS Ext2Mount(PDEVICE_OBJECT BackingDevice)
{
	// TODO
	LogMsg("Ext2: Attempt to mount from %p", BackingDevice);
	return STATUS_UNIMPLEMENTED;
}
