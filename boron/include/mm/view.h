/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/view.h
	
Abstract:
	This header defines function prototypes for the memory
	view manager.
	
Author:
	iProgramInCpp - 5 April 2025
***/
#pragma once

#include <ex/handle.h>

BSTATUS MmMapViewOfObject(
	HANDLE MappedObject,
	void** BaseAddressOut,
	size_t* ViewSizeInOut,
	int AllocationType,
	uint64_t SectionOffset,
	int Protection
);
