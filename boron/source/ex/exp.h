/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/exp.h
	
Abstract:
	This header file defines private executive support data,
	and includes some header files that are usually included.
	
Author:
	iProgramInCpp - 10 December 2023
***/
#pragma once
#include <ex.h>
#include <ke.h>
#include <ob.h>
#include <string.h>

#define EX_MUTEX_LEVEL 0

extern POBJECT_TYPE
	ExMutexObjectType,
	ExEventObjectType,
	ExSemaphoreObjectType,
	ExTimerObjectType;

bool ExpCreateMutexType();
bool ExpCreateEventType();
bool ExpCreateSemaphoreType();
bool ExpCreateTimerType();
bool ExpCreateThreadType();
bool ExpCreateProcessType();

#include <ex/internal.h>
