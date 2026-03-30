/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ps/pslist.c
	
Abstract:
	This module implements the list of processes for the
	process list in Boron.
	
Author:
	iProgramInCpp - 27 February 2026
***/
#include "psp.h"
#include <ex.h>

#define INITIAL_TABLE_SIZE       256
#define TABLE_GROWTH_SIZE        64
#define MAX_PROCESSES            1048576 // I don't think we'll run half the amount, like, ever.
#define PROCESS_LIST_MUTEX_LEVEL 5       // high because we don't really use any other mutexes here

// NOTE: the handle table has its own mutex, so no need to create another.
static void* PspProcessHandleTable;

void* PspGetProcessHandleTable()
{
	return PspProcessHandleTable;
}

INIT
void PspInitializeProcessList()
{
	BSTATUS Status = ExCreateHandleTable(
		INITIAL_TABLE_SIZE,
		TABLE_GROWTH_SIZE,
		MAX_PROCESSES,
		PROCESS_LIST_MUTEX_LEVEL,
		&PspProcessHandleTable
	);

	if (FAILED(Status)) {
		KeCrash("Failed to create process handle table: %s", RtlGetStatusString(Status));
	}
	
	PspAddProcessToList(&PsSystemProcess);
}

BSTATUS PspAddProcessToList(PEPROCESS Process)
{
	Process->ProcessId = HANDLE_NONE;
	
	BSTATUS Status;
	HANDLE Handle;
	Status = ExCreateHandle(PspProcessHandleTable, Process, &Handle);
	if (FAILED(Status))
		return Status;
	
	Process->ProcessId = Handle;
	return STATUS_SUCCESS;
}

static bool PspRemoveHandleHandler(UNUSED void* Pointer, UNUSED void* Context)
{
	// dummy
	return true;
}

void PspRemoveProcessFromList(PEPROCESS Process)
{
	if (Process->ProcessId == HANDLE_NONE)
		return;

	BSTATUS Status;
	Status = ExDeleteHandle(PspProcessHandleTable, Process->ProcessId, PspRemoveHandleHandler, NULL);
	ASSERT(SUCCEEDED(Status) && "Could not delete process handle");

	Process->ProcessId = HANDLE_NONE;
	return;
}

bool PspDebugDumpFilter(void* Pointer, UNUSED void* Context)
{
	PEPROCESS Process = (PEPROCESS) Pointer;
	
	DbgPrint("Process %p %d, called '%s'.  RC: %d", Process, Process->ProcessId, Process->ImageName, OBJECT_GET_HEADER(Process)->NonPagedObjectHeader->PointerCount);
	
	// Also dump its threads, if it has any.
	for (PLIST_ENTRY Entry = Process->Pcb.ThreadList.Flink;
		Entry != &Process->Pcb.ThreadList;
		Entry = Entry->Flink)
	{
		PKTHREAD Thread = CONTAINING_RECORD(Entry, KTHREAD, EntryProc);
		DbgPrint("\tThread %p, status %d.", Thread, Thread->Status);
	}
	
	return false; // do not delete
}

void PspDumpProcessList()
{
	DbgPrint("Dumping process list...");
	
	ExFilterHandleTable(
		PspProcessHandleTable,
		PspDebugDumpFilter,
		NULL,  // KillHandleMethod
		NULL,  // FilterContext
		NULL   // KillContext
	);
}
