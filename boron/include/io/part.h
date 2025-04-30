/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This header defines prototypes for the partition manager
	API in Boron.
	
Author:
	iProgramInCpp - 30 April 2025
***/
#pragma once

void IoRegisterPartitionableDevice(PDEVICE_OBJECT DeviceObject);

void IoRegisterFileSystemDriver(PIO_DISPATCH_TABLE DispatchTable);
