/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/traps.c
	
Abstract:
	This header file implements support for the IDT (Interrupt
	Dispatch Table).
	
Author:
	iProgramInCpp - 28 December 2025
***/
#include <arch.h>
#include <string.h>
#include <ke.h>
#include <hal.h>
