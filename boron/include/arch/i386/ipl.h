/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	arch/i386/ipl.h
	
Abstract:
	This header file contains the constant IPL definitions
	for the i386 platform.
	
Author:
	iProgramInCpp - 14 October 2025
***/
#ifndef BORON_ARCH_IPL_I386_H
#define BORON_ARCH_IPL_I386_H

// TODO: Currently copied from AMD64.
typedef enum KIPL_tag
{
	IPL_UNDEFINED = -1,
	IPL_NORMAL   = 0x0, // business as usual
	IPL_APC      = 0x3, // asynch procedure calls. Page faults only allowed up to this IPL
	IPL_DPC      = 0x4, // deferred procedure calls and the scheduler
	IPL_DEVICES0 = 0x5, // tier 1 for devices (keyboard, mouse)
	IPL_DEVICES1 = 0x6, // tier 2 for devices
	IPL_DEVICES2 = 0x7,
	IPL_DEVICES3 = 0x8,
	IPL_DEVICES4 = 0x9,
	IPL_DEVICES5 = 0xA,
	IPL_DEVICES6 = 0xB,
	IPL_DEVICES7 = 0xC,
	IPL_DEVICES8 = 0xD,
	IPL_CLOCK    = 0xE, // for clock timers
	IPL_NOINTS   = 0xF, // total control of the CPU. Interrupts are disabled in this IPL and this IPL only.
	IPL_COUNT,
}
KIPL, *PKIPL;

#endif//BORON_ARCH_IPL_I386_H
