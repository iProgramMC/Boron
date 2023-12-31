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

#ifdef TARGET_AMD64
#include <arch/amd64/ipl.h>
#else
#error Hello
#endif

#endif//BORON_ARCH_IPL_H
