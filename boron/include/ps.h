/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps.h
	
Abstract:
	This header file contains the definitions related to
	the Boron process management system.
	
Author:
	iProgramInCpp - 26 November 2023
***/
#pragma once

#include <ke.h>
#include <ex.h>
#include <pss.h>
#include <ps/process.h>
#include <ps/thread.h>
#include <ps/services.h>

#ifdef KERNEL

extern POBJECT_TYPE
	PsThreadObjectType,
	PsProcessObjectType;

#else

// TODO: PsGetBuiltInType, ExGetBuiltInType

#endif
