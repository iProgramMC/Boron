/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/ref.c
	
Abstract:
	This module implements reference counting for objects
	managed by the object manager.
	
Author:
	iProgramInCpp - 23 December 2023
***/
#include "obp.h"


void ObpReferenceObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	Hdr->NonPagedObjectHeader->PointerCount++;
}

void ObpDereferenceObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	Hdr->NonPagedObjectHeader->PointerCount--;
}
