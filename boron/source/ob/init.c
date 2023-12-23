/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/init.c
	
Abstract:
	This module implements the initialization code for the
	object manager.
	
Author:
	iProgramInCpp - 7 December 2023
***/
#include "obp.h"

bool ObpInitializeBasicMutexes()
{
	extern KMUTEX ObpObjectTypeMutex;
	extern KMUTEX ObpRootDirectoryMutex;
	
	KeInitializeMutex(&ObpObjectTypeMutex, OB_MUTEX_LEVEL_OBJECT_TYPES);
	KeInitializeMutex(&ObpRootDirectoryMutex, OB_MUTEX_LEVEL_DIRECTORY);
	
	return true;
}

bool ObInitSystem()
{
	if (!ObpInitializeBasicMutexes())
		return false;
	
	if (!ObpInitializeBasicTypes())
		return false;
	
	DbgPrint("Object manager was initialized successfully!");
	
	return true;
}
