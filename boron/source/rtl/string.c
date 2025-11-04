/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	rtl/string.c
	
Abstract:
	This module implements the platform independent part of the
	printing code.
	
Author:
	iProgramInCpp - 20 August 2023
***/

#include <main.h>

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
#if defined KERNEL && defined DEBUG
	if (!s)
		KeCrash("strlen(NULL)");
#endif
	// Count the amount of non-zero characters in the null-terminated string.
	size_t sz = 0;
	while (*s++) sz++;
	return sz;
}

char* strcpy(char* s, const char * d)
{
	return (char*)memcpy(s, d, strlen(d) + 1);
}

char* strncpy(char* dst, const char* src, size_t szBuf)
{
	for (size_t i = 0; i < szBuf; i++)
	{
		dst[i] = *src;
		if (*src)
			src++;
	}
	
	return dst;
}

char* StringCopySafe(char* dst, const char* src, size_t szBuf)
{
	if (szBuf == 0)
		return NULL;
	
	size_t i;
	
	// Copy the actual data, up until the source string ends or szBuf is depleted.
	for (i = 0; i < szBuf && src[i] != '\0'; i++)
		dst[i] = src[i];
	
	// If the size is already completely used, clear the
	// last byte of the buffer.  This function shall not
	// generate unterminated strings.
	if (i == szBuf)
		dst[szBuf - 1] = 0;
	
	// Otherwise, copy the null terminator.
	else
		dst[i] = src[i], i++;
	
#ifdef SECURE
	// Clear the rest of destination buffer for the secure system version.
	for (; i < sz; i++)
		dst[i] = 0;
#endif
	
	return dst;
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

char* strchr(const char* str, int c)
{
	while (*str)
	{
		if (*str == c)
			return (char*) str;
		
		str++;
	}
	
	return NULL;
}

bool StringMatchesCaseInsensitive(const char* String1, const char* String2, size_t Length)
{
	while (Length--)
	{
		char Ch1 = *String1;
		char Ch2 = *String2;
		
		if (Ch1 >= 'a' && Ch1 <= 'z') Ch1 = Ch1 - 'a' + 'A';
		if (Ch2 >= 'a' && Ch2 <= 'z') Ch2 = Ch2 - 'a' + 'A';
		
		if (Ch1 != Ch2)
			return false;
		
		String1++;
		String2++;
	}
	
	return true;
}

bool StringContainsCaseInsensitive(const char* Haystack, const char* Needle)
{
	size_t NeedleLength = strlen(Needle);
	size_t HaystackLength = strlen(Haystack);
	
	for (size_t i = 0; i + NeedleLength <= HaystackLength; i++)
	{
		if (StringMatchesCaseInsensitive(Haystack + i, Needle, NeedleLength))
			return true;
	}
	
	return false;
}
