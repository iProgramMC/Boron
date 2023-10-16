/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/stats.c
	
Abstract:
	This module file implements the statistics object.
	
Author:
	iProgramInCpp - 14 October 2023
***/
#include <ke.h>

KSTATISTICS KiStatistics;

void KeStatsAddContextSwitch()
{
	AtAddFetch(KiStatistics.ContextSwitches, 1);
}

int KeStatsGetContextSwitchCount()
{
	return AtLoad(KiStatistics.ContextSwitches);
}
