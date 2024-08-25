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
	
	UNUSED BSTATUS Stat =
	KeWaitForSingleObject(&Table->Mutex, false, TIMEOUT_INFINITE);
	
	ASSERT(Stat == STATUS_SUCCESS);
}

void ExUnlockHandleTable(void* TableV)
{
	PEHANDLE_TABLE Table = TableV;
	KeReleaseMutex(&Table->Mutex);
}

BSTATUS ExCreateHandleTable(size_t InitialSize, size_t GrowBySize, size_t Limit, int MutexLevel, void** OutTable)
{
	PEHANDLE_TABLE Table = MmAllocatePool(POOL_NONPAGED, sizeof(EHANDLE_TABLE));
	
	if (Table == NULL)
		return STATUS_INSUFFICIENT_MEMORY;
	
	if (Limit < InitialSize)
	{
		Limit = InitialSize;
		GrowBySize = 0;
	}
	
	KeInitializeMutex(&Table->Mutex, MutexLevel);
	
	Table->Capacity = Table->InitialSize = InitialSize;
	Table->GrowBy   = GrowBySize;
	Table->Limit    = Limit;
	
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
			return STATUS_INSUFFICIENT_MEMORY;
		}
		
		for (size_t i = 0; i < InitialSize; i++)
		{
			Table->HandleMap[i].Pointer = NULL;
		}
	}
	
	*OutTable = Table;
	return STATUS_SUCCESS;
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

static bool ExpCheckHandleCorrectness(HANDLE Handle)
{
	// A handle is invalid if:
	// - It is HANDLE_NONE, or
	// - It isn't aligned to 4.  This is an arbitrary requirement, but it may help catch invalid handles.
	return Handle != HANDLE_NONE && (Handle & 0x3) == 0;
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

BSTATUS ExDeleteHandleTable(void* Table)
{
	ExLockHandleTable(Table);
	
	if (ExIsEmptyHandleTable(Table))
	{
		ExUnlockHandleTable(Table);
		return STATUS_TABLE_NOT_EMPTY;
	}
	
	ExUnlockHandleTable(Table);
	ExpDeleteHandleTable(Table);
	return STATUS_SUCCESS;
}

BSTATUS ExKillHandleTable(void* TableV, EX_KILL_HANDLE_ROUTINE KillHandleRoutine, void* Context)
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
				return STATUS_DELETE_CANCELED;
			
			Table->HandleMap[i].Pointer = NULL;
		}
	}
	
	// Now delete the handle table itself.
	ExUnlockHandleTable(TableV);
	ExpDeleteHandleTable(TableV);
	return STATUS_SUCCESS;
}

static BSTATUS ExpResizeHandleTable(PEHANDLE_TABLE Table, size_t NewSize)
{
	// If the new size is the same as the current capacity, just return a success value.
	if (NewSize == Table->Capacity)
		return STATUS_SUCCESS;
	
	if (NewSize > Table->Limit && Table->Limit > 0)
	{
		NewSize = Table->Limit;
		
	#ifdef DEBUG
		ASSERT(NewSize >= Table->Capacity);
	#endif
		
		// If the size is unchanged after the limit, then we have too many handles.
		if (NewSize <= Table->Capacity)
			return STATUS_TOO_MANY_HANDLES;
	}
	
	DbgPrint("Resizing handle table %p to %zu", Table, NewSize);
	
	PEHANDLE_ITEM NewHandleMap = MmAllocatePool(POOL_NONPAGED, sizeof(EHANDLE_ITEM) * NewSize);
	if (!NewHandleMap)
	{
		// Error, can't expand because we ran out of memory.
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	if (NewSize > Table->Capacity)
	{
		size_t GrowBy = NewSize - Table->Capacity;
		
		if (Table->HandleMap)
		{
			// Initialize the newly allocated area with zero.
			memset(NewHandleMap + Table->Capacity, 0, GrowBy * sizeof(EHANDLE_ITEM));
			memcpy(NewHandleMap, Table->HandleMap, Table->Capacity * sizeof(EHANDLE_ITEM));
		}
		else
		{
			memset(NewHandleMap, 0, NewSize * sizeof(EHANDLE_ITEM));
			
		#ifdef DEBUG
			for (size_t i = NewSize; i < Table->Capacity; i++)
				ASSERT(Table->HandleMap[i].Pointer == NULL);
		#endif
		}
	}
	else
	{
		memcpy(NewHandleMap, Table->HandleMap, NewSize * sizeof(EHANDLE_ITEM));
	}
	
	// Free the old handle map and place in the new one.
	if (Table->HandleMap)
		MmFreePool(Table->HandleMap);
	
	Table->HandleMap = NewHandleMap;
	Table->Capacity  = NewSize;
	
	return STATUS_SUCCESS;
}

BSTATUS ExCreateHandle(void* TableV, void* Pointer, PHANDLE OutHandle)
{
	if (!Pointer)
	{
		// You can't use a NULL pointer in a handle.
		return STATUS_INVALID_PARAMETER;
	}
	
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
			*OutHandle = INDEX_TO_HANDLE(i);
			
			if (Table->MaxIndex < i)
				Table->MaxIndex = i;
			
			ExUnlockHandleTable(Table);
			return STATUS_SUCCESS;
		}
	}
	
	// We didn't find a free spot currently!
	
	// If the table may not be grown, return a none handle.
	if (!Table->GrowBy)
	{
		ExUnlockHandleTable(Table);
		return STATUS_TOO_MANY_HANDLES;
	}
	
	// Get the first unallocated handle. All handles 0 to the old capacity are allocated,
	// and all handles from the old capacity to the new one aren't, so the old capacity
	// is our first index.
	size_t NewIndex = Table->Capacity;
	
	BSTATUS Status = ExpResizeHandleTable(Table, Table->Capacity + Table->GrowBy);
	if (FAILED(Status))
	{
		// Error, can't expand! Out of memory!!
		ExUnlockHandleTable(Table);
		return Status;
	}
	
	// Allocate it!
	Table->HandleMap[NewIndex].Pointer = Pointer;
	
	if (Table->MaxIndex < NewIndex)
		Table->MaxIndex = NewIndex;
	
	*OutHandle = INDEX_TO_HANDLE(NewIndex);
	ExUnlockHandleTable(Table);
	return STATUS_SUCCESS;
}

BSTATUS ExGetPointerFromHandle(void* TableV, HANDLE Handle, void** OutObject)
{
	// Check if the handle is valid.
	if (!ExpCheckHandleCorrectness(Handle))
		return STATUS_INVALID_HANDLE;
	
	PEHANDLE_TABLE Table = TableV;
	ExLockHandleTable(Table);
	
	Handle = HANDLE_TO_INDEX(Handle);
	
	if (Handle >= Table->Capacity)
	{
		// Handle index is bigger than the table's size.
		ExUnlockHandleTable(Table);
		return STATUS_INVALID_HANDLE;
	}
	
	void* Object = Table->HandleMap[Handle].Pointer;
	*OutObject = Object;
	
	if (!Object)
	{
		ExUnlockHandleTable(Table);
		return STATUS_INVALID_HANDLE;
	}
	
	return STATUS_SUCCESS;
}

BSTATUS ExDeleteHandle(void* TableV, HANDLE Handle, EX_KILL_HANDLE_ROUTINE KillHandleRoutine, void* Context)
{
	// Check if the handle is valid.
	if (!ExpCheckHandleCorrectness(Handle))
		return STATUS_INVALID_HANDLE;
	
	Handle = HANDLE_TO_INDEX(Handle);
	
	PEHANDLE_TABLE Table = TableV;
	ExLockHandleTable(Table);
	
	if (Handle >= Table->Capacity)
	{
		// Handle index is bigger than the table's size.
		ExUnlockHandleTable(Table);
		return STATUS_INVALID_HANDLE;
	}
	
	// Try to delete the handle.
	if (!KillHandleRoutine(Table->HandleMap[Handle].Pointer, Context))
	{
		// Nope, couldn't delete it.
		ExUnlockHandleTable(Table);
		return STATUS_DELETE_CANCELED;
	}
	
	Table->HandleMap[Handle].Pointer = NULL;
	
	if (Table->MaxIndex == Handle)
	{
		// Find a lower max index.
		for (; Table->MaxIndex > 0 && Table->HandleMap[Table->MaxIndex].Pointer == NULL; Table->MaxIndex--);
		
	#ifdef DEBUG
		
		// Check if there are any entries that we skipped over in debug mode.
		for (size_t i = Table->MaxIndex + 1; i < Table->Capacity; i++)
		{
			ASSERT(Table->HandleMap[i].Pointer == NULL);
		}
		
	#endif
		
		size_t NewCapacity = Table->Capacity;
		
		// While we can bite GrowBy entries off of the handle map, we should.
		// TODO: Redundant conditions maybe?
		while (Table->MaxIndex + Table->GrowBy <= NewCapacity &&
		       Table->InitialSize + Table->GrowBy <= NewCapacity &&
		       Table->GrowBy < NewCapacity)
		{
			NewCapacity -= Table->GrowBy;
		}
		
		if (NewCapacity != Table->Capacity)
		{
			// TODO: Dropping the return result right now because an OOM condition is not fatal -
			// the handle table just won't get shrunk. We really should have an MmReallocatePool
			// type function.
			(void) ExpResizeHandleTable(Table, NewCapacity);
		}
	}
	
	ExUnlockHandleTable(Table);
	return STATUS_SUCCESS;
}
