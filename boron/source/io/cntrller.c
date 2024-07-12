/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/cntrller.c
	
Abstract:
	This module implements the I/O Controller object type.
	
Author:
	iProgramInCpp - 26 February 2024
***/
#include "iop.h"

BSTATUS IoCreateController(
	PDRIVER_OBJECT DriverObject,
	size_t ControllerExtensionSize,
	const char* ControllerName,
	UNUSED bool Permanent,
	PCONTROLLER_OBJECT* OutControllerObject
)
{
	PCONTROLLER_OBJECT ControllerObject = NULL;
	BSTATUS Status;
	
	Status = ObCreateObject(
		(void**) &ControllerObject,
		IoDevicesDir,
		IoControllerType,
		ControllerName,
		OB_FLAG_PERMANENT,
		NULL,
		sizeof (CONTROLLER_OBJECT) + ControllerExtensionSize
	);
	// ^^^^ TODO: All controllers are permanent for now.  Implement IopDeleteController
	//            and then come back and fix this.
	
	if (FAILED(Status))
		return Status;
	
	*OutControllerObject = ControllerObject;
	
	// Initialize the controller object.
	ControllerObject->DriverObject = DriverObject;
	KeInitializeMutex(&ControllerObject->DeviceTreeMutex, 1);
	InitializeAvlTree(&ControllerObject->DeviceTree);
	
	return STATUS_SUCCESS;
}

static void IopEnterControllerMutex(PCONTROLLER_OBJECT Controller)
{
	UNUSED BSTATUS Status = KeWaitForSingleObject(&Controller->DeviceTreeMutex, false, TIMEOUT_INFINITE);
	
	ASSERT(Status == STATUS_SUCCESS);
}

static void IopLeaveControllerMutex(PCONTROLLER_OBJECT Controller)
{
	KeReleaseMutex(&Controller->DeviceTreeMutex);
}

PDEVICE_OBJECT IoGetDeviceController(
	PCONTROLLER_OBJECT Controller,
	uintptr_t Key
)
{
	IopEnterControllerMutex(Controller);
	
	PAVLTREE_ENTRY Entry = LookUpItemAvlTree(&Controller->DeviceTree, Key);
	if (!Entry)
	{
		IopLeaveControllerMutex(Controller);
		return NULL;
	}
	
	IopLeaveControllerMutex(Controller);
	return CONTAINING_RECORD(Entry, DEVICE_OBJECT, DeviceTreeEntry);
}

BSTATUS IoAddDeviceController(
	PCONTROLLER_OBJECT Controller,
	uintptr_t Key,
	PDEVICE_OBJECT Device
)
{
	Device->DeviceTreeEntry.Key = Key;
	
	IopEnterControllerMutex(Controller);
	bool Inserted = InsertItemAvlTree(&Controller->DeviceTree, &Device->DeviceTreeEntry);
	
	if (Inserted)
		AtStore(Device->ParentController, Controller);
	
	IopLeaveControllerMutex(Controller);
	
	return Inserted ? STATUS_SUCCESS : STATUS_ALREADY_LINKED;
}

BSTATUS IoRemoveDeviceController(
	PCONTROLLER_OBJECT Controller,
	PDEVICE_OBJECT Device
)
{
	IopEnterControllerMutex(Controller);
	bool Removed = RemoveItemAvlTree(&Controller->DeviceTree, &Device->DeviceTreeEntry);
	if (Removed)
		AtStore(Device->ParentController, NULL);
	IopLeaveControllerMutex(Controller);
	
	return Removed ? STATUS_SUCCESS : STATUS_NOT_LINKED;
}

void IopDeleteController(void* Object)
{
	// TODO
	DbgPrint("UNIMPLEMENTED: IopDeleteController(%p)", Object);
}
