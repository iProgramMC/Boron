/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                          printf.c                           *
\*************************************************************/
#include <loader.h>
#include "charout.h"

#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_DECORATE        // don't decorate
#include <_stb_sprintf.h>

#undef TARGET_AMD64

void* memcpy(void* dst, const void* src, size_t n)
{
#ifdef TARGET_AMD64
	void* dst2 = dst;
	ASM("rep movsb":"+c"(n),"+D"(dst),"+S"(src)::"memory");
	return dst2;
#else
	uint8_t* dstp = dst;
	const uint8_t* srcp = src;
	while (n--)
		*dstp++ = *srcp++;
	return dst;
#endif
}

void* memquadcpy(uint64_t* dst, const uint64_t* src, size_t n)
{
#ifdef TARGET_AMD64
	void* dst2 = dst;
	ASM("rep movsq":"+c"(n),"+D"(dst),"+S"(src)::"memory");
	return dst2;
#else
	uint64_t* dstp = dst;
	const uint64_t* srcp = src;
	while (n--)
		*dstp++ = *srcp++;
	return dst;
#endif
}

void* memset(void* dst, int c, size_t n)
{
#ifdef TARGET_AMD64
	void* dst2 = dst;
	ASM("rep stosb":"+c"(n),"+D"(dst):"a"(c):"memory");
	return dst2;
#else
	uint8_t* dstp = dst;
	while (n--)
		*dstp++ = c;
	return dst;
#endif
}

size_t strlen(const char * s)
{
	//optimization hint : https://github.com/bminor/glibc/blob/master/string/strlen.c
	size_t sz = 0;
	while (*s++) sz++;
	return sz;
}

char* strcpy(char* s, const char * d)
{
	return (char*)memcpy(s, d, strlen(d) + 1);
}

char* strcat(char* s, const char * src)
{
	return strcpy(s + strlen(s), src);
}

int memcmp(const void* s1, const void* s2, size_t n)
{
	uint8_t* b1 = (uint8_t*)s1;
	uint8_t* b2 = (uint8_t*)s2;
	
	while (n--)
	{
		if (*b1 < *b2)
			return -1;
		if (*b1 > *b2)
			return 1;
		
		b1++, b2++;
	}
	
	// the memory is equal
	return 0;
}

int strcmp(const char* s1, const char* s2)
{
	while (true)
	{
		if (!*s1 && !*s2)
			return 0;
		
		// if either one of the pointers points to a 0 character,
		// either case applies, so it's fine.
		if (*s1 < *s2)
			return -1;
		if (*s1 > *s2)
			return 1;
		
		s1++;
		s2++;
	}
	return 0;
}

void PrintMessage(const char* Message)
{
	while (*Message)
	{
		PrintChar(*Message);
		Message++;
	}
}

void LogMsg(const char* Format, ...)
{
	// This is fine as we don't have multiple threads and all APs are parked in the trampoline.
	// The kernel will activate them, not us.
	static char Message[4096];
	
	va_list va;
	va_start(va, Format);
	Message[sizeof Message - 3] = 0;
	int chars = vsnprintf(Message, sizeof Message - 3, Format, va);
	strcpy(Message + chars, "\n");
	va_end(va);
	
	PrintMessage(Message);
}

void Crash(const char* Format, ...)
{
	static char Message[4096];
	
	va_list va;
	va_start(va, Format);
	Message[sizeof Message - 3] = 0;
	int chars = vsnprintf(Message, sizeof Message - 3, Format, va);
	strcpy(Message + chars, "\n");
	va_end(va);
	
	// TODO
	PrintMessage(Message);
	ASM("cli");
	while (true)
		ASM("hlt");
}

