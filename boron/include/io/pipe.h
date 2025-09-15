/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	io/pipe.h
	
Abstract:
	This header defines APIs related to I/O pipe objects.
	
Author:
	iProgramInCpp - 15 September 2025
***/
#pragma once

BSTATUS IoCreatePipe(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, size_t BufferSize, bool NonBlock);

BSTATUS OSCreatePipe(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, size_t BufferSize, bool NonBlock);
