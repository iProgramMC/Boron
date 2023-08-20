// Boron - Main include file.
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

#define BIT(x) (1ULL << (x))

#define ASM __asm__ __volatile__

// We're using C11
#define static_assert _Static_assert

// Include stb printf so we have definitions ready
#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_DECORATE        // don't decorate
#include "_stb_sprintf.h"

void LogMsg(const char* msg, ...);
void SLogMsg(const char* msg, ...);

#endif//NS64_MAIN_H
