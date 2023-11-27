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

BSTATUS ObCreateDirectoryObject(
	POBJECT_DIRECTORY* DirectoryOut,
	POBJECT_ATTRIBUTES ObjectAttributes)
{
	POBJECT_DIRECTORY Directory;
	BSTATUS Status;
	KPROCESSOR_MODE PreviousMode;
	void* DirectoryAddr;
	
	PreviousMode = KeGetPreviousMode();
	
	// Allocate and initialize the directory object.
	Status = ObCreateObject(
		ObpDirectoryObjectType,
		ObjectAttributes,
		PreviousMode,
		NULL,
		sizeof (*Directory),
		&DirectoryAddr
	);
	
	Directory = DirectoryAddr;
	
	if (Status)
		return Status;
	
	memset(Directory, 0, sizeof *Directory);
	
	*DirectoryOut = Directory;
	
	return STATUS_SUCCESS;
}
