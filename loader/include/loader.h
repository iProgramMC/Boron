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

void* memcpy(void* dst, const void* src, size_t n);
void* memquadcpy(uint64_t* dst, const uint64_t* src, size_t n);
void* memset(void* dst, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
size_t strlen(const char * s);
char* strcpy(char* s, const char * d);
char* strcat(char* s, const char * src);
int strcmp(const char* s1, const char* s2);

#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_DECORATE        // don't decorate
#include <_stb_sprintf.h>
