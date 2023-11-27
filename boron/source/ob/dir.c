/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/dir.c
	
Abstract:
	This module implements the directory object type.
	
Author:
	iProgramInCpp - 27 November 2023
***/
#include "obp.h"

BSTATUS
ObCreateDirectoryObject(
	PHANDLE DirHandleOut,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes)
{
	//POBJECT_DIRECTORY Directory;
	HANDLE Handle;
	BSTATUS Status;
	
	
	// Allocate and initialize the directory object.
	//Status = ObCreateObject(
	
	return 0;
}
