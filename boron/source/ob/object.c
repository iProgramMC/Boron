/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/object.c
	
Abstract:
	This module implements common functions to handle objects.
	
Author:
	iProgramInCpp - 8 December 2023
***/
#include "obp.h"

#ifdef DEBUG
BSTATUS ObiDebugObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	POBJECT_TYPE Type = Hdr->NonPagedObjectHeader->ObjectType;
	
	if (!Type->TypeInfo.Debug)
		return STATUS_SUCCESS; // Unimplemented.
	
	return Type->TypeInfo.Debug(Object);
}
#endif

BSTATUS ObpDeleteObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	POBJECT_TYPE Type = Hdr->NonPagedObjectHeader->ObjectType;
	
	if (Type->TypeInfo.Delete)
		Type->TypeInfo.Delete(Object);
	
	PNONPAGED_OBJECT_HEADER NpHdr = Hdr->NonPagedObjectHeader;
	
	if (Type->TypeInfo.NonPagedPool)
	{
		MmFreePool(NpHdr);
	}
	else
	{
		MmFreePool(NpHdr);
		MmFreePool(Hdr);
	}
	
	return STATUS_SUCCESS;
}
