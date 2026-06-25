/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	portinit.c
	
Abstract:
	This module implements the local IPC system's initialization
	routines.
	
Author:
	iProgramInCpp - 13 June 2026
***/
#include "ipci.h"

POBJECT_TYPE IpcPortObjectType, IpcConnectionObjectType;

BSTATUS IpcCreatePortObjectTypes()
{
	BSTATUS Status = ObCreateObjectType(
		"Port",
		&IpcPortObjectTypeInfo,
		&IpcPortObjectType
	);
	
	if (FAILED(Status))
		DbgPrint("Failed to create port object type: %s (%d)", RtlGetStatusString(Status), Status);

	Status = ObCreateObjectType(
		"Connection",
		&IpcConnectionObjectTypeInfo,
		&IpcConnectionObjectType
	);
	
	if (FAILED(Status))
		DbgPrint("Failed to create connection object type: %s (%d)", RtlGetStatusString(Status), Status);

	return Status;
}

bool IpcInitSystem()
{
	if (FAILED(IpcCreatePortObjectTypes()))
		return false;
	
	return true;
}

