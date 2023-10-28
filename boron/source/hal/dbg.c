/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/dbg.c
	
Abstract:
	This module implements the debug terminal functions for
	the AMD64 platform.
	
Author:
	iProgramInCpp - 15 October 2023
***/
#include <hal.h>

#ifdef DEBUG
void DbgPrintString(const char* str);
#endif

void HalPrintStringDebug(const char* str)
{
#ifdef DEBUG
	DbgPrintString(str);
#endif
}
