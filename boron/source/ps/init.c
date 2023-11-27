/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/init.c
	
Abstract:
	This module implements the initialization code for the
	process manager in Boron.
	
Author:
	iProgramInCpp - 26 November 2023
***/
#include <ps.h>

static EPROCESS PsSystemProcess;

PEPROCESS PsGetSystemProcess()
{
	return &PsSystemProcess;
}

