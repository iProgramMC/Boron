/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the main function for the
	iPod touch GPIO device driver.
	
Author:
	iProgramInCpp - 16 March 2026
***/
#include <io.h>
#include <string.h>

PDRIVER_OBJECT gDriverObject;

BSTATUS DriverEntry(PDRIVER_OBJECT DriverObject)
{
	gDriverObject = DriverObject;
	DbgPrint("Hello from iPod GPIO driver!");
	return STATUS_SUCCESS;
}
