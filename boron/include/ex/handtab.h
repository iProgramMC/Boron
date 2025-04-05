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

#include "handle.h"

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
	size_t InitialSize;
	size_t MaxIndex;
	size_t Limit;
	
	PEHANDLE_ITEM HandleMap;
}
EHANDLE_TABLE, *PEHANDLE_TABLE;

typedef bool(*EX_KILL_HANDLE_ROUTINE)(void* Pointer, void* Context);

typedef void*(EX_DUPLICATE_HANDLE_METHOD)(void* Handle, void* Context);

// Creates a handle table.
// If GrowBySize is equal to zero, the handle table cannot grow, so attempts to
// ExCreateHandle will return HANDLE_NONE.
BSTATUS ExCreateHandleTable(size_t InitialSize, size_t GrowBySize, size_t Limit, int MutexLevel, void** OutTable);

// Acquires the handle table's mutex.
void ExLockHandleTable(void* HandleTable);

// Releases the handle table's mutex.
void ExUnlockHandleTable(void* HandleTable);

// Creates a handle within the handle table.
BSTATUS ExCreateHandle(void* HandleTable, void* Pointer, PHANDLE OutHandle);

// Maps a handle into a pointer, using the specified handle table.  If a pointer was returned
// through *OutObject, then the handle table remains locked, in which case ExUnlockHandleTable
// must be called.  The handle table is unlocked when an error occurs.  In that case, the
// ExUnlockHandleTable function must NOT be called.
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

// Creates a new handle table with the same initial conditions as the old one.
// Does not duplicate any handles.
BSTATUS ExCreateHandleTableInherit(void** NewHandleTable, void* HandleTable);

// Duplicates a handle table.
//
// Creates a handle table of the same size as the provided handle table, and calls the
// EX_DUPLICATE_HANDLE_METHOD on each one.
//
// NOTE that DuplicateMethod will be called with the old handle table locked.
BSTATUS ExDuplicateHandleTable(void** NewHandleTable, void* HandleTable, EX_DUPLICATE_HANDLE_METHOD DuplicateMethod, void* Context);

// Duplicates a handle in the handle table.
//
// NOTE that DuplicateMethod will be called with the old handle table locked.
BSTATUS ExDuplicateHandle(void* HandleTable, HANDLE Handle, PHANDLE OutHandle, EX_DUPLICATE_HANDLE_METHOD DuplicateMethod, void* Context);

// Duplicates a handle in the handle table by specifying the new handle's value.
//
// This function exists to support the implementation of the dup2() standard C library call.
BSTATUS ExDuplicateHandleToHandle(
	void* HandleTable,
	HANDLE Handle,
	HANDLE NewHandle,
	EX_DUPLICATE_HANDLE_METHOD DuplicateMethod,
	EX_KILL_HANDLE_ROUTINE KillHandleMethod,
	void* DuplicateContext,
	void* KillContext);