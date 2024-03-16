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

POBJECT_TYPE IopDriverType;
OBJECT_TYPE_INFO IopDriverTypeInfo;

bool IopInitializeDriverType()
{
	// TODO: Initialize IopDriverTypeInfo
	
	BSTATUS Status = ObCreateObjectType(
		"Driver",
		&IopDriverTypeInfo,
		&IopDriverType
	);
	
	if (FAILED(Status))
	{
		DbgPrint("IO: Failed to create Driver type");
		return false;
	}
	
	return true;
}
