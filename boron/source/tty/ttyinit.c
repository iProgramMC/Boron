/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ttyinit.c
	
Abstract:
	This module implements the TTY (Virtual Teletype)
	object type initialization function.
	
Author:
	iProgramInCpp - 29 October 2025
***/
#include "ttyi.h"

POBJECT_TYPE TtyTerminalObjectType;

BSTATUS TtyInitializeTerminalObjectType()
{
	BSTATUS Status = ObCreateObjectType(
		"Terminal",
		&TtyTerminalObjectTypeInfo,
		&TtyTerminalObjectType
	);
	
	if (FAILED(Status))
		DbgPrint("Failed to create terminal object type.");
	
	return Status;
}
