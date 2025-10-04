/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	main.h
	
Abstract:
	This header file contains the global definitions
	for the Boron kernel and drivers.
	
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
#define NO_DISCARD    __attribute__((warn_unused_result))
#include <status.h>

#define FORCE_INLINE ALWAYS_INLINE static inline
#define BIT(x) (1ULL << (x))
#define ASM __asm__ __volatile__

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define HIDDEN __attribute__((visibility("hidden")))

#ifdef DEBUG
#define CHECK_PAGED do {           \
	if (KeGetIPL() >= IPL_APC) {   \
		KeCrash("%s: Running at IPL %d > IPL_APC", KeGetIPL(), IPL_APC); \
	}                              \
} while (0)
#else
#define CHECK_PAGED
#endif


#ifdef __cplusplus

extern "C" {

#else

// We're using C11
#define static_assert _Static_assert

#endif

void LogMsg(const char* msg, ...);

#ifdef DEBUG
void DbgPrint(const char* msg, ...);
#else
#define DbgPrint(...)
#endif

#ifdef __cplusplus
}
#endif

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

#define IN
#define OUT
#define INOUT
#define OPTIONAL

#define CallerAddress() ((uintptr_t) __builtin_return_address(0))

#define CONTAINING_RECORD(Pointer, Type, Field) ((Type*)((uintptr_t)(Pointer) - (uintptr_t)offsetof(Type, Field)))

#include <list.h>
#include <rtl/check64.h>

#endif//NS64_MAIN_H
