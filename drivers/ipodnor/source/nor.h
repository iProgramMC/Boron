/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	nor.h

Abstract:
	.

Author:
	iProgramInCpp - 24 October 2026
***/
#pragma once

#include <ke.h>
#include <ldr.h>
#include <io.h>
#include <mm.h>

extern IO_DISPATCH_TABLE NorCfiDispatchTable;
extern IO_DISPATCH_TABLE NorSpiDispatchTable;

extern PDRIVER_OBJECT gDriverObject;
extern PDEVICE_OBJECT gDeviceObject;

bool NorCfiInit();
bool NorSpiInit();

size_t NorSize();

#define REG(base, rgof) (* (volatile uint32_t*) ((uintptr_t)base + (rgof)))
#define REG16(base, rgof) (* (volatile uint16_t*) ((uintptr_t)base + (rgof)))
