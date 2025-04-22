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
#ifdef KERNEL
#include <main.h>
#else
#include <boron.h>
#endif

#ifndef KERNEL

NO_RETURN
void RtlAbort()
{
	DbgPrint("** ABORTED\n");
	
	// TODO:
	//OSExitProcess();
	OSExitThread();
}

#endif // !KERNEL

#ifdef DEBUG

NO_RETURN
bool RtlAssert(const char* Condition, const char* File, int Line, const char* Message)
{
#ifdef KERNEL
	KeCrash("Assertion failed: %s%s%s%s\nAt %s:%d",
	        Condition,
	        Message ? " (" : "",
	        Message ? Message : "",
	        Message ? ")" : "",
	        File,
	        Line);
#else
	// TODO
	DbgPrint("Assertion failed: %s%s%s%s\nAt %s:%d",
	         Condition,
	         Message ? " (" : "",
	         Message ? Message : "",
	         Message ? ")" : "",
	         File,
	         Line);
	
	RtlAbort();
#endif
}

#endif // DEBUG
