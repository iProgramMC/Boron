/***
	The Boron Operating System
	Copyright (C) 2024-2025 iProgramInCpp

Module name:
	ex/handle.h
	
Abstract:
	This header file defines the handle type.
	
	It was part of ex/handtab.h but it was decided to
	relocate it.
	
Author:
	iProgramInCpp - 5 April 2025
***/
#pragma once
#include <stdint.h>

typedef uintptr_t HANDLE, *PHANDLE;
