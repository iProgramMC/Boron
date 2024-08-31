/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/process.h
	
Abstract:
	This module implements the process manager process
	object in Boron.
	
Author:
	iProgramInCpp - 30 August 2024
***/
#include "psp.h"

void PspDeleteProcess(void* ProcessV)
{
	UNUSED PEPROCESS Process = ProcessV;
	
	// TODO:
	// I suppose that, when a process is started, the reference count should be biased by one,
	// and when the process is finished, the reference count is decremented.  This way, PspDeleteProcess
	// would only be called if the process object has no more running threads.
	ASSERT(IsListEmpty(&Process->Pcb.ThreadList));
	
	DbgPrint("TODO: PspDeleteProcess");
}

INIT
bool PsCreateProcessType()
{
	OBJECT_TYPE_INFO ObjectInfo;
	memset (&ObjectInfo, 0, sizeof ObjectInfo);
	
	ObjectInfo.NonPagedPool = true;
	ObjectInfo.MaintainHandleCount = true;
	
	ObjectInfo.Delete = PspDeleteProcess;
	
	BSTATUS Status = ObCreateObjectType(
		"Process",
		&ObjectInfo,
		&PsProcessObjectType
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Could not create Process type: %d", Status);
		return false;
	}
	
	return true;
}
