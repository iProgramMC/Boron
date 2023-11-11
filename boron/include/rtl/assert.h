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

#ifdef DEBUG

bool RtlAssert(const char* Condition, const char* File, int Line, const char* Message);

#define ASSERT(Condition)           ((void)((Condition) ? 0 : RtlAssert(#Condition, __FILE__, __LINE__, NULL)))
#define ASSERT2(Condition, Message) ((void)((Condition) ? 0 : RtlAssert(#Condition, __FILE__, __LINE__, Message)))

#else

// Define ASSERT and ASSERT2 as empty macros. A release build does not have assertions of any kind.
#define ASSERT(...)
#define ASSERT2(...)

#endif

#endif//BORON_RTL_ASSERT_H
