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

#include <ex/process.h>

PEPROCESS PsGetSystemProcess();

void PsInitSystemProcess();

bool PsInitSystem();
