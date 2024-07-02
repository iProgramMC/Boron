/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/rdwr.h
	
Abstract:
	This header defines the I/O manager's read-write file operation
	functions.
	
Author:
	iProgramInCpp - 2 July 2024
***/
#pragma once

#include <ob.h>
#include "iosb.h"

BSTATUS IoReadFile(
	PIO_STATUS_BLOCK Iosb,
	HANDLE Handle,
	void* Buffer,
	size_t Length,
	bool CanBlock
);

BSTATUS IoWriteFile(
	PIO_STATUS_BLOCK Iosb,
	HANDLE Handle,
	const void* Buffer,
	size_t Length,
	bool CanBlock
);
