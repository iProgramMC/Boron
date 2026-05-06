/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	hal/i386hals.h
	
Abstract:
	This header file contains the definitions for the i386-specific
	HAL callbacks.
	
Author:
	iProgramInCpp - 6 May 2026
***/
#pragma once

#ifndef TARGET_I386
#error Do not include this!
#endif

void HalPicRegisterInterrupt(uint8_t Vector, KIPL Ipl);

void HalPicDeregisterInterrupt(uint8_t Vector, KIPL Ipl);
