/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	rtl/check64.h
	
Abstract:
	This header file defines IS_64_BIT on 64-bit platforms, and
	IS_32_BIT on 32-bit platforms.
	
Author:
	iProgramInCpp - 30 April 2024
***/
#pragma once

#if defined TARGET_AMD64

#define IS_64_BIT 1

#elif defined TARGET_I386 || defined TARGET_ARM

#define IS_32_BIT 1

#else

#error Add your platform here!

#endif

// In the future it should look something like this:
// #if defined TARGET_AMD64 || defined TARGET_RISCV64 || defined TARGET_AARCH64
// #    define IS_64_BIT
// #else if defined TARGET_I486 || defined TARGET_ARM || defined TARGET_MIPS
// #    define IS_32_BIT
// #else
// #    error Add your platform here!
// #endif
