/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/term.c
	
Abstract:
	This module implements the terminal functions for the i386
	platform.
	
Author:
	iProgramInCpp - 17 October 2025
***/
#include <ke.h>
#include <string.h>
#include "hali.h"

HAL_API void HalDisplayString(const char* Message)
{
	// TODO
	DbgPrint("HalDisplayString(%zu): %s", strlen(Message), Message);
}
