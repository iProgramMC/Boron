/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	string.h
	
Abstract:
	This header file contains the definitions for the
	runtime string library.
	
Author:
	iProgramInCpp - 20 August 2023
***/
#ifndef NS64_STRING_H
#define NS64_STRING_H

#include <stdint.h>

// Include stb printf so we have definitions ready
#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_DECORATE        // don't decorate
#include "_stb_sprintf.h"

void* memcpy(void* dst, const void* src, size_t n);
void* memquadcpy(uint64_t* dst, const uint64_t* src, size_t n);
void* memset(void* dst, int c, size_t n);
size_t strlen(const char * s);
char* strcpy(char* s, const char * d);
char* strcat(char* s, const char * src);
int memcmp(const void* s1, const void* s2, size_t n);
int strcmp(const char* s1, const char* s2);
char* strncpy(char* d, const char* s, size_t sz);

#endif//NS64_STRING_H