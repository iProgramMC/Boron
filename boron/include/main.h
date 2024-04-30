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

#define BIT(x) (1ULL << (x))

#define ASM __asm__ __volatile__

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
#include <rtl/avltree.h>

#ifdef IS_DRIVER

// Force the driver entry prototype.
typedef struct _DRIVER_OBJECT DRIVER_OBJECT, *PDRIVER_OBJECT;
BSTATUS DriverEntry(PDRIVER_OBJECT Object);

#endif

#endif//NS64_MAIN_H
