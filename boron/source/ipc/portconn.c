/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	portconn.c
	
Abstract:
	This module implements the local IPC port connection object.
	
Author:
	iProgramInCpp - 17 June 2026
***/
#include "ipci.h"

void IpcRejectConnectionRequest(PIPC_CONNECTION_REQUEST Request, BSTATUS Status)
{
	// We shouldn't need to do this, as there's no way a connection could be
	// both accepted or denied at the same time.
#ifdef SECURE
	KeResetEvent(&Request->AcceptedEvent);
#endif
	
	Request->RejectedReason = Status;
	KeSetEvent(&Request->RejectedEvent, IPC_BOOST_LEVEL);
}

void IpcDeleteConnection(void* ObjectV)
{
	PIPC_PORT_CONNECTION Connection = ObjectV;
	
	// Remove ourselves from the IPC connection queue.
	PIPC_PORT Port = Connection->Port;
	KeWaitForSingleObject(&Port->ConnectionsMutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	
	RemoveItemRbTree(&Port->Connections, &Connection->ConnectionEntry);
	
	if (Port->HostConnection == Connection)
		Port->HostConnection = NULL;
	
	KeReleaseMutex(&Port->ConnectionsMutex);
	
	// Remove our reference to the port object.
	ObDereferenceObject(Port);
	
	// We actually don't need to lock the message list mutex anymore since we're
	// the only reference to the connection object anyway.
	
	// Dismiss all the messages in the queue.
	while (!IsListEmpty(&Connection->Messages.MessageListHead))
	{
		PIPC_MESSAGE Message = CONTAINING_RECORD(
			Connection->Messages.MessageListHead.Flink,
			IPC_MESSAGE,
			ListEntry
		);
		
		RemoveEntryList(&Message->ListEntry);
		
		IpcDeleteMessage(Message);
	}
	
	// Dismiss all connection requests.
	while (!IsListEmpty(&Connection->Messages.ConnectionRequestListHead))
	{
		PIPC_CONNECTION_REQUEST Request = CONTAINING_RECORD(
			Connection->Messages.ConnectionRequestListHead.Flink,
			IPC_CONNECTION_REQUEST,
			ListEntry
		);
		
		RemoveEntryList(&Request->ListEntry);
		
		// Ownership of connection requests is actually with the caller in question, so
		// there's no need to free the connection request or anything.  If anything, it's
		// likely allocated on the caller's stack.
		IpcRejectConnectionRequest(Request, STATUS_PORT_CLOSED);
	}
}

OBJECT_TYPE_INFO IpcConnectionObjectTypeInfo =
{
	.NonPagedPool = false,
	.MaintainHandleCount = false,
	.Delete = IpcDeleteConnection
};
