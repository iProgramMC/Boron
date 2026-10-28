/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	nor.h

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 24 October 2026
***/
#pragma once

#include <ke.h>
#include <ldr.h>
#include <io.h>
#include <mm.h>

enum {
	M68AP,
	N45AP,
	N82AP,
	N72AP,
	N88AP,
	K48AP
};
extern int NorModel;

extern IO_DISPATCH_TABLE NorCfiDispatchTable;
extern IO_DISPATCH_TABLE NorSpiDispatchTable;

extern PDRIVER_OBJECT gDriverObject;
extern PDEVICE_OBJECT gDeviceObject;

bool NorCfiInit();
bool NorSpiInit();

size_t NorSize();

#define REG(base, rgof) (* (volatile uint32_t*) ((uintptr_t)base + (rgof)))
#define REG16(base, rgof) (* (volatile uint16_t*) ((uintptr_t)base + (rgof)))

int NorSpiCsPin();
int NorSpiPort();
size_t NorMaxReadBytes();
