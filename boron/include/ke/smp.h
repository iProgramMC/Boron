/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/smp.h
	
Abstract:
	This header file contains the definitions related
	to the SMP part of the kernel core.
	
Author:
	iProgramInCpp - 28 October 2023
***/
#pragma once

#include <main.h>

NO_RETURN void KeInitSMP(void);
void KeInitArchUP();
void KeInitArchMP();
