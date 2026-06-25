/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ipc.h
	
Abstract:
	This header file contains declarations for the Boron local
	interprocess communication (IPC) port object and its interfaces.
	
Author:
	iProgramInCpp - 13 June 2026
***/
#pragma once

#include <mm.h>

#define MAX_SMALL_MESSAGE (65280)

typedef struct tagIPC_PORT_CONNECTION IPC_PORT_CONNECTION, *PIPC_PORT_CONNECTION;
typedef struct tagIPC_PORT IPC_PORT, *PIPC_PORT;

typedef struct
{
	// The entry into the list of pending messages.
	LIST_ENTRY ListEntry;
	
	// The code for the message.  This code is application-specific and the kernel
	// passes it along verbatim.
	int MessageCode;
	
	// A message may have an object associated with it.  The client passes an
	// arbitrary HANDLE, which this message references and passes along to the
	// server.
	void* AssociatedObject;
	
	// For large messages, use a section instead.
	//
	// A section can be reused for multiple messages at the client and server's
	// discretion, so the kernel will not ensure that messages don't overlap and
	// that they don't get overwritten between transfers.
	PMMSECTION AssociatedSection;
	
	uint64_t SectionViewOffset;
	uint64_t SectionViewSize;
	
	// For small messages, a contiguous buffer is stored alongside the message,
	// in a single allocation.
	uint16_t BufferSize;
	char Buffer[];
}
IPC_MESSAGE, *PIPC_MESSAGE;

typedef struct
{
	// The entry into the list of pending connection requests.
	LIST_ENTRY ListEntry;
	
	// Events signalled upon a rejection/acceptance of the connection.
	BSTATUS RejectedReason;
	KEVENT RejectedEvent;
	KEVENT AcceptedEvent;
	
	// The client process trying to connect.
	PEPROCESS ClientProcess;
}
IPC_CONNECTION_REQUEST, *PIPC_CONNECTION_REQUEST;

typedef struct
{
	// An event that's SIGNALLED when there are messages in the list. This is a
	// notification event.
	//
	// This event is placed at the TOP of IPC_MESSAGE_QUEUE, and the IPC_MESSAGE_QUEUE
	// structure is also placed at the top of the IPC_PORT_CONNECTION and IPC_PORT
	// structures.  This is to allow OSWaitForMultipleObjects/OSWaitForSingleObject
	// to function (and wait on the correct KEVENT) without additional code.
	KEVENT PendingMessagesEvent;
	
	// Mutex guarding the message list lock.
	KMUTEX MessageListMutex;
	
	// The list of messages sent to this process.
	LIST_ENTRY MessageListHead;
	
	// The list of connection requests sent to this process.
	// Only used for connection ID 0.
	LIST_ENTRY ConnectionRequestListHead;
}
IPC_MESSAGE_QUEUE, *PIPC_MESSAGE_QUEUE;

struct tagIPC_PORT_CONNECTION
{
	// This MUST be the first item in this structure to allow OSWaitForObjects
	// calls to function properly without additional code.
	IPC_MESSAGE_QUEUE Messages;
	
	// The port this connection relates to.
	PIPC_PORT Port;
	
	// Entry within the tree of connections in the IPC port object.
	RBTREE_ENTRY ConnectionEntry;
	
	// The key of the ConnectionEntry contains the ID of the connection.
};

struct tagIPC_PORT
{
	// Mutex guarding the list of connections.
	KMUTEX ConnectionsMutex;
	
	// The list of connections, indexed by connection ID.
	RBTREE Connections;
	
	// A shortcut to the connection with ID of 0.
	PIPC_PORT_CONNECTION HostConnection;
	
	// The next connection ID.
	//
	// Connection 0 is reserved for the host, and subsequent connection
	// IDs are created for each accepted connection request.
	uint64_t NextConnectionID;
};

// ==== Exposed Interface ====



#ifdef KERNEL

bool IpcInitSystem();

#endif
