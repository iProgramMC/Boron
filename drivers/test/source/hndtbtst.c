/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	hndtbtst.h
	
Abstract:
	This module implements the handle table test.
	
Author:
	iProgramInCpp - 7 January 2024
***/
#include <ex.h>

bool OnKillHandle(void* HandleToKill, UNUSED void* Context)
{
	LogMsg("Killing handle with pointer: %p", HandleToKill);
	return true;
}

void PerformHandleTest()
{
	void* HanTab;
	if (FAILED(ExCreateHandleTable(0, 1, 4, 1, &HanTab)))
		KeCrash("Error, handle table must exist to perform the test");
	
	void *Ptr1, *Ptr2, *Ptr3, *Ptr4, *Ptr5;
	Ptr1 = &Ptr1;
	Ptr2 = &Ptr2;
	Ptr3 = &Ptr3;
	Ptr4 = &Ptr4;
	Ptr5 = &Ptr5;
	
	LogMsg("Pointer 1: %p", Ptr1);
	LogMsg("Pointer 2: %p", Ptr2);
	LogMsg("Pointer 3: %p", Ptr3);
	LogMsg("Pointer 4: %p", Ptr4);
	LogMsg("Pointer 5: %p", Ptr5);
	
	// Add each pointer in the handle table.
	// N.B. The first four allocations must succeed, and the 5th one may fail
	HANDLE Hand1 = 0, Hand2 = 0, Hand3 = 0, Hand4 = 0, Hand5 = 0;
	
	(void) ExCreateHandle(HanTab, Ptr1, &Hand1);
	(void) ExCreateHandle(HanTab, Ptr2, &Hand2);
	(void) ExCreateHandle(HanTab, Ptr3, &Hand3);
	(void) ExCreateHandle(HanTab, Ptr4, &Hand4);
	(void) ExCreateHandle(HanTab, Ptr5, &Hand5);
	
	if (!Hand1 || !Hand2 || !Hand3 || !Hand4)
		LogMsg("One of the handles is missing");
	
	if (Hand5)
		LogMsg("Handle 5 exists");
	else
		LogMsg("Handle 5 does not exist");
	
	LogMsg("Handle 1: %p", Hand1);
	LogMsg("Handle 2: %p", Hand2);
	LogMsg("Handle 3: %p", Hand3);
	LogMsg("Handle 4: %p", Hand4);
	LogMsg("Handle 5: %p", Hand5);
	
	// Try killing the fifth handle:
	if (SUCCEEDED(ExDeleteHandle(HanTab, Hand5, OnKillHandle, NULL)))
		LogMsg("Deleting handle 5 succeeded");
	else
		LogMsg("Deleting handle 5 failed.");
	
	// Try killing the fourth handle:
	if (SUCCEEDED(ExDeleteHandle(HanTab, Hand4, OnKillHandle, NULL)))
		LogMsg("Deleting handle 4 succeeded");
	else
		LogMsg("Deleting handle 4 failed.");
	
	// Kill the handle table.
	if (SUCCEEDED(ExKillHandleTable(HanTab, OnKillHandle, NULL)))
		LogMsg("Handle table was destroyed");
	else
		LogMsg("Handle table could not be destroyed");
}
