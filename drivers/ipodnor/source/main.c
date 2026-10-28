/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	main.c

Abstract:
	This module implements the main function for the
	iPod/iPhone NOR flash driver.

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
#include "nor.h"
#include <string.h>
#include <ps.h>

PDRIVER_OBJECT gDriverObject;
PDEVICE_OBJECT gDeviceObject;

int NorModel;

bool UseSPIAccess()
{
	return NorModel != M68AP && NorModel != N45AP;
}

size_t NorSize()
{
	return 1 * 1024 * 1024;
}

BSTATUS DriverEntry(PDRIVER_OBJECT DriverObject)
{
	gDriverObject = DriverObject;

	const char* Model = KeLoaderParameterBlock.HardwareIdentifier;
	if (strcmp(Model, "m68ap") == 0) {
		NorModel = M68AP;
	} else if (strcmp(Model, "n45ap") == 0) {
		NorModel = N45AP;
	} else if (strcmp(Model, "n82ap") == 0) {
		NorModel = N82AP;
	} else if (strcmp(Model, "n72ap") == 0) {
		NorModel = N72AP;
	} else if (strcmp(Model, "n88ap") == 0) {
		NorModel = N88AP;
	} else if (strcmp(Model, "k48ap") == 0) {
		NorModel = K48AP;
	} else {
		LogMsg("Hardware configuration %s not supported.  Assuming N72AP", Model);
		NorModel = N72AP;
	}

	PIO_DISPATCH_TABLE DispatchTable;

	bool Result = false;
	if (UseSPIAccess()) {
		DispatchTable = &NorSpiDispatchTable;
		Result = NorSpiInit();
	}
	else {
		DispatchTable = &NorCfiDispatchTable;
		Result = NorCfiInit();
	}

	if (!Result)
	{
		LogMsg("NOR initialization failed.");
		return STATUS_UNLOAD;
	}

	BSTATUS Status = IoCreateDevice(
		gDriverObject,
		0,  // DeviceExtensionSize
		0,  // FcbExtensionSize
		"NorFlash",
		DEVICE_TYPE_BLOCK,
		true, // Permanent
		DispatchTable,
		&gDeviceObject
	);

	if (FAILED(Status))
	{
		LogMsg("NOR: Failed to create device object: %s", RtlGetStatusString(Status));
		return STATUS_UNLOAD;
	}

	// Assign it a size manually.
	gDeviceObject->Fcb->FileLength = NorSize();

	return STATUS_SUCCESS;
}
