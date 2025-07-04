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

INIT
bool ObpInitializeBasicMutexes()
{
	extern KMUTEX ObpObjectTypeMutex;
	
	KeInitializeMutex(&ObpObjectTypeMutex, OB_MUTEX_LEVEL_OBJECT_TYPES);
	
	return true;
}

INIT
bool ObInitSystem()
{
	if (!ObpInitializeBasicMutexes())
		return false;
	
	if (!ObpInitializeBasicTypes())
		return false;
	
	if (!ObpInitializeRootDirectory())
		return false;
	
	if (!ObpInitializeReaperThread())
		return false;
	
	DbgPrint("Object manager was initialized successfully!");
	
	return true;
}
