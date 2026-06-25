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

void IpcDeleteMessage(PIPC_MESSAGE Message)
{
	// Relinquish ownership over the associated object and
	// section.
	if (Message->AssociatedObject)
		ObDereferenceObject(Message->AssociatedObject);
	
	if (Message->AssociatedSection)
		ObDereferenceObject(Message->AssociatedSection);
	
	// Then free the message.
	MmFreePool(Message);
}
