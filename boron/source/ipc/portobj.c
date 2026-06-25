/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	portobj.c
	
Abstract:
	This module implements the local IPC port object.
	
Author:
	iProgramInCpp - 17 June 2026
***/
#include "ipci.h"

void IpcDeletePort(void* ObjectV)
{
	PIPC_PORT Port = ObjectV;
	
	// The connections tree should be empty.
	//
	// A connection holds a reference to the port, so if all connections
	// are closed, then the whole port is deleted.
	ASSERT(IsEmptyRbTree(&Port->Connections));
	
	// Also, the port object, in and of itself, doesn't really hold any
	// other data to be allocated, so the object can simply be freed.
	//
	// However, in case there *will* be data to reference from the port
	// object, we'll add it here.
}

OBJECT_TYPE_INFO IpcPortObjectTypeInfo =
{
	.NonPagedPool = false,
	.MaintainHandleCount = false,
	.Delete = IpcDeletePort
};
