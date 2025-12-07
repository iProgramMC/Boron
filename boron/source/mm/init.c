/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/init.c
	
Abstract:
	This module implements a small part of the memory manager
	initialization process.
	
	This one initializes the object types used in the memory
	manager.
	
Author:
	iProgramInCpp - 7 December 2025
***/
#include "mi.h"

POBJECT_TYPE MmSectionObjectType;
POBJECT_TYPE MmOverlayObjectType;

static OBJECT_TYPE_INFO MmSectionObjectTypeInfo =
{
	.NonPagedPool = true,
	.MaintainHandleCount = false,
	.Delete = MmDeleteSectionObject
};

static OBJECT_TYPE_INFO MmOverlayObjectTypeInfo =
{
	.NonPagedPool = true,
	.MaintainHandleCount = false,
	.Delete = MmDeleteOverlayObject
};

bool MmInitSystem()
{
	BSTATUS Status = ObCreateObjectType(
		"Section",
		&MmSectionObjectTypeInfo,
		&MmSectionObjectType
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Failed to create Section object type: %s (%d)", RtlGetStatusString(Status), Status);
		return false;
	}
	
	Status = ObCreateObjectType(
		"Overlay",
		&MmOverlayObjectTypeInfo,
		&MmOverlayObjectType
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Failed to create Overlay object type: %s (%d)", RtlGetStatusString(Status), Status);
		return false;
	}
	
	return true;
}
