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
#include "tar.h"

BSTATUS TmpMountTarFile(PLOADER_MODULE Module)
{
	PTAR_UNIT CurrentUnit = Module->Address;
	PTAR_UNIT EndUnit = Module->Address + Module->Size;
	
	while (CurrentUnit < EndUnit)
	{
		CurrentUnit++;
	}
}
