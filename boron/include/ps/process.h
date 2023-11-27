/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/process.h
	
Abstract:
	This header file defines the EPROCESS structure, which is an
	extension of KPROCESS, and is exposed by the object manager.
	
Author:
	iProgramInCpp - 26 November 2023
***/
#pragma once

#include <ke/process.h>

typedef struct EPROCESS_tag
{
	KPROCESS Pcb;
	
	// TODO
}
EPROCESS, *PEPROCESS;

PEPROCESS PsGetSystemProcess();
