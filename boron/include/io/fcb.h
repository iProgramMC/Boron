/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/fcb.h
	
Abstract:
	
	
Author:
	iProgramInCpp - 22 June 2024
***/
#pragma once

typedef struct _FCB
{
	PDEVICE_OBJECT DeviceObject;
	
	int ReferenceCount;
}
FCB, *PFCB;
