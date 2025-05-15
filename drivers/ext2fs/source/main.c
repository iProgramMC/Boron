/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the main function of the
	Ext2 File System driver.
	
Author:
	iProgramInCpp - 30 April 2025
***/
#include "ext2.h"

IO_DISPATCH_TABLE Ext2DispatchTable =
{
	.Mount = Ext2Mount,
};

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	IoRegisterFileSystemDriver(&Ext2DispatchTable);
	return STATUS_SUCCESS;
}
