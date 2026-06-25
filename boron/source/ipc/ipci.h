#pragma once

#include <ob.h>
#include <mm.h>
#include <ipc.h>

#define IPC_BOOST_LEVEL 2

extern OBJECT_TYPE_INFO IpcPortObjectTypeInfo;
extern OBJECT_TYPE_INFO IpcConnectionObjectTypeInfo;

extern POBJECT_TYPE IpcPortObjectType;
extern POBJECT_TYPE IpcConnectionObjectType;

// Deletes a message.
void IpcDeleteMessage(PIPC_MESSAGE Message);

// Rejects a connection request because of a specified status.
void IpcRejectConnectionRequest(PIPC_CONNECTION_REQUEST Request, BSTATUS Status);

PIPC_PORT_CONNECTION IpcTryApproveConnectionRequest(PIPC_PORT Port, PIPC_CONNECTION_REQUEST Request);
