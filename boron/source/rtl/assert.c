/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	rtl/assert.c
	
Abstract:
	This module implements the handler for failed assertions
	in debug mode.
	
Author:
	iProgramInCpp - 11 November 2023
***/
#include <main.h>
#ifdef DEBUG

bool RtlAssert(const char* Condition, const char* File, int Line, const char* Message)
{
	KeCrash("Assertion failed: %s%s%s%s\nAt %s:%d",
	        Condition,
	        Message ? " (" : "",
	        Message ? Message : "",
	        Message ? ")" : "",
	        File,
	        Line);
}

#endif
