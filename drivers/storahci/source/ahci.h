/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ahci.h
	
Abstract:
	This header defines data types and includes common system
	headers used by the AHCI storage device driver.
	
Author:
	iProgramInCpp - 7 July 2024
***/
#include <main.h>
#include <ke.h>
#include <io.h>
#include <hal.h>

typedef struct
{
	int Test;
}
DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct
{
	int Test;
}
FCB_EXTENSION, *PFCB_EXTENSION;
