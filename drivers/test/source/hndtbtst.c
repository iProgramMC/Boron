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

bool OnKillHandle2(void* HandleToKill, UNUSED void* Context)
{
	LogMsg("Killing2 handle with pointer: %p", HandleToKill);
	return true;
}

void* OnDuplicateHandle(void* Object, UNUSED void* Context)
{
	LogMsg("OnDuplicateHandle: %p", Object);
	return Object;
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
	
	// Try to clone the handle table.
	void* NewHanTab = NULL;
	BSTATUS Status = ExDuplicateHandleTable(&NewHanTab, HanTab, OnDuplicateHandle, NULL);
	if (FAILED(Status))
		KeCrash("Failed to duplicate handle table: %d", Status);
	
	// In this new handle table, duplicate handle number 2.
	Status = ExDuplicateHandle(NewHanTab, Hand2, &Hand5, OnDuplicateHandle, NULL);
	if (FAILED(Status))
		KeCrash("Failed to duplicate handle 2: %d", Status);
	
	// Try to do that again, this time it should fail
	Status = ExDuplicateHandle(NewHanTab, Hand1, &Hand5, OnDuplicateHandle, NULL);
	if (Status != STATUS_TOO_MANY_HANDLES)
		KeCrash("ExDuplicateHandle should not have succeeded (status %d)", Status);
	else
		LogMsg("ExDuplicateHandle failed properly this time");
	
	// Kill the handle table.
	if (SUCCEEDED(ExKillHandleTable(HanTab, OnKillHandle, NULL)))
		LogMsg("Handle table was destroyed");
	else
		LogMsg("Handle table could not be destroyed");
	
	// Kill the other handle table.
	if (SUCCEEDED(ExKillHandleTable(NewHanTab, OnKillHandle2, NULL)))
		LogMsg("New handle table was destroyed");
	else
		LogMsg("New handle table could not be destroyed");
}
