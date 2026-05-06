/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	hal/amd64hal.h
	
Abstract:
	This header file contains the definitions for the AMD64-specific
	HAL callbacks.
	
Author:
	iProgramInCpp - 6 May 2026
***/
#pragma once

#ifndef TARGET_AMD64
#error Do not include this!
#endif

void HalIoApicSetIrqRedirect(uint8_t Vector, uint8_t Irq, uint32_t LapicId, bool Status);
