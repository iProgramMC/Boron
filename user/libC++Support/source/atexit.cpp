//
//  atexit.cpp
//
//  Copyright (C) 2026 iProgramInCpp.
//  Created on 9/4/2026.
//
//  Implements the __cxa_atexit call.
//
#include <boron.h>

// Specify it's weak so that we can override it if needed.
//
// (Ideally, nobody linking things with mlibc would also link in libC++Support though)
extern "C"
__attribute__((weak))
int __cxa_atexit(void (*Function)(void*), void* Argument, void* DsoHandle)
{
	DbgPrint("__cxa_atexit(%p, %p, %p)", Function, Argument, DsoHandle);
	BSTATUS Status = OSRegisterExitCallback(Function, Argument, DsoHandle);
	if (FAILED(Status))
		return -1;
	
	return 0;
}
