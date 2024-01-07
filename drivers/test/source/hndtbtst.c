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

bool OnKillHandle(void* HandleToKill, void* Context)
{
	LogMsg("Killing handle with pointer: %p", HandleToKill);
	return true;
}

void PerformHandleTest()
{
	void* HanTab = ExCreateHandleTable(4, 0, 1);
	if (!HanTab)
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
	HANDLE Hand1 = ExCreateHandle(HanTab, Ptr1);
	HANDLE Hand2 = ExCreateHandle(HanTab, Ptr2);
	HANDLE Hand3 = ExCreateHandle(HanTab, Ptr3);
	HANDLE Hand4 = ExCreateHandle(HanTab, Ptr4);
	HANDLE Hand5 = ExCreateHandle(HanTab, Ptr5);
	
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
	if (ExDeleteHandle(HanTab, Hand5, OnKillHandle, NULL))
		LogMsg("Deleting handle 5 succeeded");
	else
		LogMsg("Deleting handle 5 failed.");
	
	// Try killing the third handle:
	if (ExDeleteHandle(HanTab, Hand3, OnKillHandle, NULL))
		LogMsg("Deleting handle 3 succeeded");
	else
		LogMsg("Deleting handle 3 failed.");
	
	// Kill the handle table.
	if (ExKillHandleTable(HanTab, OnKillHandle, NULL))
		LogMsg("Handle table was destroyed");
	else
		LogMsg("Handle table could not be destroyed");
}
