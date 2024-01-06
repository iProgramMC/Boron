/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/handtab.c
	
Abstract:
	This module implements the handle table feature of the
	executive.
	
Author:
	iProgramInCpp - 3 January 2024
***/
#include <ex.h>
#include <string.h>

// IMPORTANT!  The handles are formatted as follows:
// (indexIntoHandleTable + 1) << 2
//
// This allows handle 0 to be invalid, and handles that aren't aligned
// to four to be marked as invalid, to catch certain user errors easier.

#define INDEX_TO_HANDLE(Index) (((Index) + 1) << 2)
#define HANDLE_TO_INDEX(Handle) (((Handle) >> 2) - 1)

void ExLockHandleTable(void* TableV)
{
	PEHANDLE_TABLE Table = TableV;
	
	ASSERT(KeWaitForSingleObject(&Table->Mutex, false, TIMEOUT_INFINITE) == STATUS_SUCCESS);
}

void ExUnlockHandleTable(void* TableV)
{
	PEHANDLE_TABLE Table = TableV;
	KeReleaseMutex(&Table->Mutex);
}

void* ExCreateHandleTable(size_t InitialSize, size_t GrowBySize, int MutexLevel)
{
	PEHANDLE_TABLE Table = MmAllocatePool(POOL_NONPAGED, sizeof(EHANDLE_TABLE));
	
	if (Table == NULL)
		return NULL;
	
	KeInitializeMutex(&Table->Mutex, MutexLevel);
	
	Table->Capacity = InitialSize;
	Table->GrowBy   = GrowBySize;
	
	if (InitialSize == 0)
	{
		Table->HandleMap = NULL;
	}
	else
	{
		Table->HandleMap = MmAllocatePool(POOL_NONPAGED, sizeof(EHANDLE_ITEM) * InitialSize);
		
		if (Table->HandleMap == NULL)
		{
			MmFreePool(Table);
			return NULL;
		}
		
		for (size_t i = 0; i < InitialSize; i++)
		{
			Table->HandleMap[i].Pointer = NULL;
		}
	}
	
	return Table;
}

static void ExpDeleteHandleTable(void* TableV)
{
	// Deletes the handle table. This involves deleting the
	// handle map, as well as the handle table itself.
	PEHANDLE_TABLE Table = TableV;
	
	if (Table->HandleMap)
		MmFreePool(Table->HandleMap);
	
	MmFreePool(Table);
}

bool ExIsEmptyHandleTable(void* TableV)
{
	PEHANDLE_TABLE Table = TableV;
	
	for (size_t i = 0; i < Table->Capacity; i++)
	{
		if (Table->HandleMap[i].Pointer != NULL)
			return false;
	}
	
	return true;
}

bool ExDeleteHandleTable(void* Table)
{
	if (ExIsEmptyHandleTable(Table))
		return false;
	
	ExpDeleteHandleTable(Table);
	return true;
}

bool ExKillHandleTable(void* TableV, EX_KILL_HANDLE_ROUTINE KillHandleRoutine, void* Context)
{
	ExLockHandleTable(TableV);
	
	PEHANDLE_TABLE Table = TableV;
	
	for (size_t i = 0; i < Table->Capacity; i++)
	{
		if (Table->HandleMap[i].Pointer != NULL)
		{
			// Delete the handle.
			if (!KillHandleRoutine(Table->HandleMap[i].Pointer, Context))
				// TODO: Handle table may have been partially deleted!
				return false;
			
			Table->HandleMap[i].Pointer = NULL;
		}
	}
	
	// Now delete the handle table itself.
	ExUnlockHandleTable(TableV);
	ExpDeleteHandleTable(TableV);
	return true;
}

HANDLE ExCreateHandle(void* TableV, void* Pointer)
{
	if (!Pointer)
		// You can't use a NULL pointer in a handle!
		return HANDLE_NONE;
	
	PEHANDLE_TABLE Table = TableV;
	ExLockHandleTable(Table);
	
	// TODO: Use a free list instead.
	// Look for a free entry in the handle table:
	for (size_t i = 0; i < Table->Capacity; i++)
	{
		if (!Table->HandleMap[i].Pointer)
		{
			// We found a spot!
			Table->HandleMap[i].Pointer = Pointer;
			ExUnlockHandleTable(Table);
			return INDEX_TO_HANDLE(i);
		}
	}
	
	// We didn't find a free spot currently!
	
	// If the table may not be grown, return a none handle.
	if (!Table->GrowBy)
	{
		ExUnlockHandleTable(Table);
		return HANDLE_NONE;
	}
	
	PEHANDLE_ITEM NewHandleMap = MmAllocatePool(POOL_NONPAGED, sizeof(EHANDLE_ITEM) * (Table->Capacity + Table->GrowBy));
	if (!NewHandleMap)
	{
		// Error, can't expand! Out of memory!!
		ExUnlockHandleTable(Table);
		return HANDLE_NONE;
	}
	
	// Initialize the newly allocated area with zero.
	memset(NewHandleMap + Table->Capacity, 0, Table->GrowBy * sizeof(EHANDLE_ITEM));
	memcpy(NewHandleMap, Table->HandleMap, Table->Capacity * sizeof(EHANDLE_ITEM));
	
	// Free the old handle map and place in the new one.
	MmFreePool(Table->HandleMap);
	Table->HandleMap = NewHandleMap;
	
	// Get the first unallocated handle. All handles 0 to the old capacity are allocated,
	// and all handles from the old capacity to the new one aren't, so the old capacity
	// is our first index.
	size_t NewIndex = Table->Capacity;
	Table->Capacity += Table->GrowBy;
	
	// Allocate it!
	Table->HandleMap[NewIndex].Pointer = Pointer;
	ExUnlockHandleTable(Table);
	
	return INDEX_TO_HANDLE(NewIndex);
}

void* ExGetPointerFromHandle(void* TableV, HANDLE Handle)
{
	PEHANDLE_TABLE Table = TableV;
	ExLockHandleTable(Table);
	
	// Check if the handle is valid in the first place.
	if (Handle == HANDLE_NONE)
		// A null handle was passed.
		return NULL;
	
	if (Handle & 0x3)
		// Not aligned to 4. Arbitrary requirement but
		// may help catch invalid handle usage.
		return NULL;
	
	Handle = HANDLE_TO_INDEX(Handle);
	
	if (Handle >= Table->Capacity)
		// Handle index is bigger than the table's size.
		return NULL;
	
	// Just return the pointer, if it's NULL, that means that it's
	// already allocated and we were going to return NULL anyway.
	return Table->HandleMap[Handle].Pointer;
}

bool ExDeleteHandle(void* TableV, HANDLE Handle, EX_KILL_HANDLE_ROUTINE KillHandleRoutine, void* Context)
{
	// Check if the handle is valid in the first place.
	if (Handle == HANDLE_NONE)
		// A null handle was passed.
		return false;
	
	if (Handle & 0x3)
		// Not aligned to 4. Arbitrary requirement but
		// may help catch invalid handle usage.
		return false;
	
	Handle = HANDLE_TO_INDEX(Handle);
	
	PEHANDLE_TABLE Table = TableV;
	ExLockHandleTable(Table);
	
	if (Handle >= Table->Capacity)
	{
		// Handle index is bigger than the table's size.
		ExUnlockHandleTable(Table);
		return false;
	}
	
	// Try to delete the handle.
	if (!KillHandleRoutine(Table->HandleMap[Handle].Pointer, Context))
	{
		// Nope, couldn't delete it.
		ExUnlockHandleTable(Table);
		return false;
	}
	
	Table->HandleMap[Handle].Pointer = NULL;
	ExUnlockHandleTable(Table);
	return true;
}
