/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                          loader.h                           *
\*************************************************************/
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#define PACKED        __attribute__((packed))
#define NO_RETURN     __attribute__((noreturn))
#define RETURNS_TWICE __attribute__((returns_twice))
#define UNUSED        __attribute__((unused))
#define ALWAYS_INLINE __attribute__((always_inline))

#include <list.h>

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(*(arr)))

#define BIT(x) (1ULL << (x))

#define ASM __asm__ __volatile__

// We're using C11
#define static_assert _Static_assert

#define CallerAddress() ((uintptr_t) __builtin_return_address(0))

#define CONTAINING_RECORD(Pointer, Type, Field) ((Type*)((uintptr_t)(Pointer) - (uintptr_t)offsetof(Type, Field)))

#ifdef DEBUG
void LogMsg(const char* Message, ...);
#else
#define LogMsg(...)
#endif

void Crash(const char* Message, ...);
