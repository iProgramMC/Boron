/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	portmsg.c
	
Abstract:
	This module implements the local IPC port message object.
	
Author:
	iProgramInCpp - 17 June 2026
***/
#include "ipci.h"
#include <string.h>

void IpcDeleteMessage(PIPC_MESSAGE Message)
{
	// Relinquish ownership over the associated object and
	// section.
	if (Message->AssociatedObject)
		ObDereferenceObject(Message->AssociatedObject);
	
	// Then free the message.
	MmFreePool(Message);
}

BSTATUS IpcCreateMessage(
	PIPC_MESSAGE* OutMessage,
	int MessageCode,
	void* MessageContent,
	size_t MessageContentSize,
	HANDLE AssociatedObject,
	HANDLE AssociatedSection,
	uint64_t SectionViewOffset,
	uint64_t SectionViewSize
)
{
	if (MessageContentSize >= MAX_SMALL_MESSAGE)
	{
		return STATUS_MESSAGE_TOO_LONG;
	}
	
	PIPC_MESSAGE Message = MmAllocatePool(POOL_PAGED, sizeof(IPC_MESSAGE) + MessageContentSize);
	if (!Message)
	{
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	BSTATUS Status = STATUS_SUCCESS;
	memset(Message, 0, sizeof (*Message));
	
	if (AssociatedObject)
	{
		void* AssociatedObjectPtr;
		Status = ObReferenceObjectByHandle(AssociatedObject, NULL, &AssociatedObjectPtr);
		if (FAILED(Status)) {
			goto Error;
		}
		
		Message->AssociatedObject = AssociatedObjectPtr;
	}
	
	
	*OutMessage = Message;
	return Status;
	
Error:
	IpcDeleteMessage(Message);
	
	return Status;
}
