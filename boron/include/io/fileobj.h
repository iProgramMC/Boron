/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/fileobj.h
	
Abstract:
	
	
Author:
	iProgramInCpp - 22 June 2024
***/
#pragma once

typedef struct _FILE_OBJECT
{
	PDEVICE_OBJECT DeviceObject;
	
	void* Context1;
	void* Context2;
	
	uint64_t Offset;
	uint32_t Flags;
}
FILE_OBJECT, *PFILE_OBJECT;
