/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	arch/i386/ipl.h
	
Abstract:
	This header file contains the constant IPL definitions
	for the i386 architecture.
	
Author:
	iProgramInCpp - 14 August 2025
***/
#pragma once

typedef enum KIPL_tag
{
	IPL_UNDEFINED = -1,
	IPL_NORMAL = 0,
	IPL_APC    = 1,
	IPL_DPC    = 2,
	IPL_DEVICES0 = 3,
	IPL_DEVICES1 = 4,
	IPL_DEVICES2 = 5,
	IPL_DEVICES3 = 6,
	IPL_DEVICES4 = 7,
	IPL_DEVICES5 = 8,
	IPL_DEVICES6 = 9,
	IPL_DEVICES7 = 10,
	IPL_DEVICES8 = 11,
	IPL_DEVICES9 = 12,
	IPL_CLOCK    = 14,
	IPL_NOINTS   = 15,
	IPL_COUNT,
}
KIPL, *PKIPL;
