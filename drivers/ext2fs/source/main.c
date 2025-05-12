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
#include <main.h>

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	LogMsg("Hello from ext2fs");
	return STATUS_SUCCESS;
}
