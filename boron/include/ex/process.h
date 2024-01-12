/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/process.h
	
Abstract:
	This header file defines the executive process structure.
	
Author:
	iProgramInCpp - 8 January 2024
***/
#pragma once

#include <ke/process.h>

typedef struct EPROCESS_tag EPROCESS, *PEPROCESS;

struct EPROCESS_tag
{
	// The kernel side process.
	KPROCESS Process;
};
