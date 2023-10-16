/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/stats.h
	
Abstract:
	This header file contains the statistics object and
	manipulation functions for it.
	
Author:
	iProgramInCpp - 14 October 2023
***/
#ifndef BORON_KE_STATS_H
#define BORON_KE_STATS_H

// Note. This object is global, that means there's one single
// instance of this object at a time and you should use atomic
// writes to write to the statistics object.
typedef struct KSTATISTICS_tag
{
	int ContextSwitches;
}
KSTATISTICS, *PKSTATISTICS;

// Modify
void KeStatsAddContextSwitch();


// Read
int KeStatsGetContextSwitchCount();

#endif//BORON_KE_STATS_H