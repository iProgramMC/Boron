/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/device.c
	
Abstract:
	This module implements the I/O File object type.
	
Author:
	iProgramInCpp - 28 April 2024
***/
#include "iop.h"

void IopDeleteFile(void* Object)
{
	// TODO
	DbgPrint("UNIMPLEMENTED: IopDeleteFile(%p)", Object);
}

void IopCloseFile(void* Object, int HandleCount)
{
	// TODO
	DbgPrint("UNIMPLEMENTED: IopCloseFile(%p, %d)", Object, HandleCount);
}

BSTATUS IopParseFile(void* Object, UNUSED const char** Name, UNUSED void* Context, UNUSED int LoopCount, UNUSED void** OutObject)
{
	// TODO
	DbgPrint("UNIMPLEMENTED: IopParseFile(%p)", Object);
	return STATUS_UNIMPLEMENTED;
}
