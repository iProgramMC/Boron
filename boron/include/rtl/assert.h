/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	rtl/assert.h
	
Abstract:
	This header file implements the assertion logic on debug
	checked builds of the Boron kernel.
	
Author:
	iProgramInCpp - 11 November 2023
***/
#ifndef BORON_RTL_ASSERT_H
#define BORON_RTL_ASSERT_H

// ASSERT: Asserts that a condition is true and crashes otherwise. On release, the condition is still
//         emitted.
//
// ASSERT2: Ditto, but displays an extra message
//
// ASSERTN: Same as ASSERT, but on release, is replaced with nothing.

#ifdef DEBUG

bool RtlAssert(const char* Condition, const char* File, int Line, const char* Message);

#define ASSERT(Condition)           ((void)((Condition) ? 0 : RtlAssert(#Condition, __FILE__, __LINE__, NULL)))
#define ASSERT2(Condition, Message) ((void)((Condition) ? 0 : RtlAssert(#Condition, __FILE__, __LINE__, Message)))
#define ASSERTN(Condition)          ((void)((Condition) ? 0 : RtlAssert(#Condition, __FILE__, __LINE__, NULL)))

#else

// Define ASSERT and ASSERT2 as a simple evaluation of the condition (which is likely to
// be optimized out). A release build does not have assertions of any kind.
//
// Define ASSERT3 as nothing.
#define ASSERT(Condition)           ((void)(Condition))
#define ASSERT2(Condition, Message) ((void)(Condition))
#define ASSERTN(Condition)

#endif

#endif//BORON_RTL_ASSERT_H
