/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/section.h
	
Abstract:
	This header defines the memory manager section object.
	
Author:
	iProgramInCpp - 7 December 2025
***/
#pragma once

#include <obs.h>

typedef struct
{
	MAPPABLE_HEADER Mappable;
	KMUTEX Mutex;
	MMSLA Sla;
	uint64_t MaxSizePages;
}
MMSECTION, *PMMSECTION;

BSTATUS MmCreateAnonymousSectionObject(
	PMMSECTION* OutSection,
	uint64_t MaxSize
);

BSTATUS OSCreateSectionObject(
	PHANDLE OutSectionHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	uint64_t MaxSize
);
