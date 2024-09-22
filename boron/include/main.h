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
#include <rtl/ansi.h>
#include <rtl/assert.h>
#include <rtl/check64.h>
#include <status.h>

#define FORCE_INLINE ALWAYS_INLINE static inline
#define BIT(x) (1ULL << (x))
#define ASM __asm__ __volatile__

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

//
// Functions are by default non-paged. Thus, they are resident at all times.
// However, two special classes of functions, defined below, may not be resident at all times:
//
// - Init.  Only resident during system initialization.  Is reclaimed by system once init is complete.
//
// - Page.  Is added to the kernel's working set.  May be paged out to the swap file, and restored.
//          Page faults may be taken during execution in this function.
//
#define INIT __attribute__((section(".text.init")))
#define PAGE __attribute__((section(".text.page")))

#ifdef DEBUG
#define CHECK_PAGED do {           \
	if (KeGetIPL() >= IPL_APC) {   \
		KeCrash("%s: Running at IPL %d > IPL_APC", KeGetIPL(), IPL_APC); \
	}                              \
} while (0)
#else
#define CHECK_PAGED
#endif

// We're using C11
#define static_assert _Static_assert

void LogMsg(const char* msg, ...);

#ifdef DEBUG
void DbgPrint(const char* msg, ...);
#else
#define DbgPrint(...)
#endif

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

#define IN
#define OUT
#define INOUT
#define OPTIONAL

#define CallerAddress() ((uintptr_t) __builtin_return_address(0))

#define CONTAINING_RECORD(Pointer, Type, Field) ((Type*)((uintptr_t)(Pointer) - (uintptr_t)offsetof(Type, Field)))

// Include only the kernel crash routines specifically
#include <ke/crash.h>

#include <rtl/list.h>
#include <rtl/rbtree.h>

// TODO: not sure this belongs here, but we'll take it.
#define KERNEL_STACK_SIZE (PAGE_SIZE * 2) // Note: Must be a multiple of PAGE_SIZE.

#ifdef IS_DRIVER

// Force the driver entry prototype.
typedef struct _DRIVER_OBJECT DRIVER_OBJECT, *PDRIVER_OBJECT;
BSTATUS DriverEntry(PDRIVER_OBJECT Object);

#endif

#endif//NS64_MAIN_H
