/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/handtab.h
	
Abstract:
	This header file defines the handle table structure and the
	associated operations that can be performed on it.
	
Author:
	iProgramInCpp - 3 January 2024
***/
#pragma once

#include <ke.h>

typedef uintptr_t HANDLE, *PHANDLE;

#define HANDLE_NONE (0)

typedef struct tagEHANDLE_ITEM
{
	void* Pointer;
}
EHANDLE_ITEM, *PEHANDLE_ITEM;

typedef struct tagEHANDLE_TABLE
{
	KMUTEX Mutex;
	
	size_t Capacity;
	size_t GrowBy;
	
	PEHANDLE_ITEM HandleMap;
}
EHANDLE_TABLE, *PEHANDLE_TABLE;

typedef bool(*EX_KILL_HANDLE_ROUTINE)(void* Pointer, void* Context);

// Creates a handle table.
// If GrowBySize is equal to zero, the handle table cannot grow, so attempts to
// ExCreateHandle will return HANDLE_NONE.
BSTATUS ExCreateHandleTable(size_t InitialSize, size_t GrowBySize, int MutexLevel, void** OutTable);

// Acquires the handle table's mutex.
void ExLockHandleTable(void* HandleTable);

// Releases the handle table's mutex.
void ExUnlockHandleTable(void* HandleTable);

// Creates a handle within the handle table. May return HANDLE_NONE if:
// - The table already has the maximal amount of handles and GrowBySize is equal to zero, and
// - The table already has the maximal amount of handles and the table could not be grown.
BSTATUS ExCreateHandle(void* HandleTable, void* Pointer, PHANDLE OutHandle);

// Maps a handle into a pointer, using the specified handle table.  This returns with the
// handle table locked, regardless of whether a pointer was actually returned or not, so
// the caller must call ExUnlockHandleTable.
BSTATUS ExGetPointerFromHandle(void* HandleTable, HANDLE Handle, void** OutObject);

// Checks if a handle table contains no elements.
bool ExIsEmptyHandleTable(void* HandleTable);

// Deletes a handle within the handle table. Returns what the KillHandleRoutine returns after
// a call.  If the handle couldn't be deleted, it remains in the handle table.
BSTATUS ExDeleteHandle(void* HandleTable, HANDLE Handle, EX_KILL_HANDLE_ROUTINE KillHandleRoutine, void* Context);

// Deletes a handle table.  Returns true if the handle was deleted, false if it wasn't.
// The table must be empty in order to be deleted.
BSTATUS ExDeleteHandleTable(void* HandleTable);

// Deletes a handle table. Unlike ExDeleteHandleTable, deletes all handles first by calling
// a caller specific routine to destroy each handle separately. Returns false if one of the
// handles could not be deleted.
BSTATUS ExKillHandleTable(void* HandleTable, EX_KILL_HANDLE_ROUTINE KillHandleRoutine, void* Context);
