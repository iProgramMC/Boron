/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/version.c
	
Abstract:
	This module implements the semantic versioning for
	the Boron kernel.
	
Author:
	iProgramInCpp - 28 September 2025
***/
#include <ke.h>
#include <mm.h>

int KeGetVersionNumber()
{
	return VER(__BORON_MAJOR, __BORON_MINOR, __BORON_BUILD);
}

BSTATUS OSGetVersionNumber(int* VersionNumber)
{
	int Number = KeGetVersionNumber();
	return MmSafeCopy(VersionNumber, &Number, sizeof(int), KeGetPreviousMode(), true);
}
