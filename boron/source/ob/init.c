/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/init.c
	
Abstract:
	This module implements the initialization code for the
	object manager.
	
Author:
	iProgramInCpp - 26 November 2023
***/
#include "obp.h"

void ObInitializeFirstPhase()
{
	OBJECT_TYPE_INITIALIZER Initializer;
	memset(&Initializer, 0, sizeof Initializer);
	
	Initializer.IsNonPagedPool = true;
	/*
	// Create Type object type
	Initializer.MaintainTypeList = true;
	
	if (ObCreateObjectType("Type", &Initializer, &ObpTypeObjectType))
		KeCrash("ObInitializeFirstPhase: could not create Type type object");
	
	Initializer.MaintainTypeList = false;
	
	// Create Directory object type
	if (ObCreateObjectType("Directory", &Initializer, &ObpDirectoryObjectType))
		KeCrash("ObInitializeFirstPhase: could not create Directory type object");
	
	// Create Symbolic Link object type
	if (ObCreateObjectType("SymbolicLink", &Initializer, &ObpSymLinkObjectType))
		KeCrash("ObInitializeFirstPhase: could not create SymbolicLink type object");*/
}

void ObInitializeSecondPhase()
{
	// Create root directory.
}
