/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io.h
	
Abstract:
	This header file contains the definitions related to
	the Boron kernel I/O manager.
	
Author:
	iProgramInCpp - 15 February 2024
***/
#pragma once

#include <main.h>
#include <ob.h>

typedef struct _DEVICE_OBJECT DEVICE_OBJECT, *PDEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT, *PDRIVER_OBJECT;
typedef struct _FILE_OBJECT FILE_OBJECT, *PFILE_OBJECT;
typedef struct _IRP IRP, *PIRP;
typedef struct _FCB FCB, *PFCB;

#include <io/irp.h>
#include <io/devobj.h>
#include <io/drvobj.h>
#include <io/fcb.h>
#include <io/fileobj.h>

// These aren't meant to be used directly. Instead, they're used
// by the IoGetBuiltInData function. If the compile unit including
// this header is not part of the kernel, what would normally link
// to extern global variables would instead call IoGetBuiltInData.
enum {
	__IO_DRIVER_TYPE = 0,
	__IO_DEVICE_TYPE = 1,
	__IO_FILE_TYPE   = 2,
	__IO_DEVICES_DIR = 3,
	__IO_DRIVERS_DIR = 4,
};

void* IoGetBuiltInData(int Number);

#ifdef KERNEL

// The kernel can use these directly.
extern POBJECT_TYPE IoDriverType, IoDeviceType, IoFileType;
extern POBJECT_DIRECTORY IoDriversDir, IoDevicesDir;

#else

// Due to a limitation of the driver dynamic linker (Ldr), have to do this:
#define IoDriverType ((POBJECT_TYPE) IoGetBuiltInData(__IO_DRIVER_TYPE))
#define IoDeviceType ((POBJECT_TYPE) IoGetBuiltInData(__IO_DEVICE_TYPE))
#define IoFileType   ((POBJECT_TYPE) IoGetBuiltInData(__IO_FILE_TYPE))
#define IoDriversDir ((POBJECT_DIRECTORY) IoGetBuiltInData(__IO_DRIVERS_DIR))
#define IoDevicesDir ((POBJECT_DIRECTORY) IoGetBuiltInData(__IO_DEVICES_DIR))

#endif

bool IoInitSystem();

