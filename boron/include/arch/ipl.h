/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	arch/amd64/ipl.h
	
Abstract:
	This header file contains includes for the IPL
	enum definition for each platform.
	
Author:
	iProgramInCpp - 8 October 2023
***/
#ifndef BORON_ARCH_IPL_H
#define BORON_ARCH_IPL_H

#if   defined TARGET_AMD64
#include <arch/amd64/ipl.h>
#elif defined TARGET_I386
#include <arch/i386/ipl.h>
#elif defined TARGET_ARM
#include <arch/arm/ipl.h>
#else
#error Implement ipl.h for your architecture!
#endif

#endif//BORON_ARCH_IPL_H
