/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	string.h
	
Abstract:
	This header file contains the definitions that
	everyone should have available at all times.
	
Author:
	iProgramInCpp - 20 August 2023
***/
#ifndef NS64_MAIN_H
#define NS64_MAIN_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#define PACKED        __attribute__((packed))
#define NO_RETURN     __attribute__((noreturn))
#define RETURNS_TWICE __attribute__((returns_twice))
#define UNUSED        __attribute__((unused))
#define ALWAYS_INLINE __attribute__((always_inline))

#define BIT(x) (1ULL << (x))

#define ASM __asm__ __volatile__

// We're using C11
#define static_assert _Static_assert

void LogMsg(const char* msg, ...);
void SLogMsg(const char* msg, ...);

#endif//NS64_MAIN_H
