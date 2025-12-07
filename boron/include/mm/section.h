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

typedef struct
{
	MAPPABLE_HEADER Mappable;
	KMUTEX Mutex;
	MMSLA Sla;
}
MMSECTION, *PMMSECTION;
