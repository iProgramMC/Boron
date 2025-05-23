/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ext2.h
	
Abstract:
	This header defines all of the structures used by
	the ext2 file system driver as well as the ext2
	I/O dispatch function prototypes.
	
Author:
	iProgramInCpp - 15 May 2025
***/
#pragma once

#include <main.h>
#include <io.h>

BSTATUS Ext2Mount(PDEVICE_OBJECT BackingDevice);
