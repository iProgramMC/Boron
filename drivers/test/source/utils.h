/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	utils.h
	
Abstract:
	This header file provides forward declarations for utilitary
	functions in the test driver module.
	
Author:
	iProgramInCpp - 19 November 2023
***/
#pragma once

#include <ke.h>

// Create a simple thread.
PKTHREAD CreateThread(PKTHREAD_START StartRoutine, void* Parameter);
