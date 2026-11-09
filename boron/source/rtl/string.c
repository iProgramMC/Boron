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

// In some cases, strlen may not access past the end of a string, using a word-aligned
// pointer.  Uncomment this to disable this behavior.
#define STRLEN_MAY_ACCESS_PAST_STRING

#define PTR_MISALIGNMENT(Ptr) ((uintptr_t)(Ptr) & (sizeof(uintptr_t) - 1))

#define PTR_PLUS(Ptr, NBytes) ((void*)((uintptr_t)(Ptr) + (NBytes)))
#define PTR_AS(Ptr, Type) ((Type*)(Ptr))

void* memcpy(void* dst, const void* src, size_t n)
{
	void* result = dst;
	uint8_t *dstp = dst;
	uintptr_t *dstpp = dst;
	const uint8_t *srcp = src;
	const uintptr_t *srcpp = src;

	if (UNLIKELY(PTR_MISALIGNMENT(dst) != PTR_MISALIGNMENT(src)))
	{
		// Pointers will never be aligned to word size.
		// TODO: try to do 16-bit copy instead?
		goto CopyByteByByte;
	}

	// Copy the beginning, until dst is aligned to word size.
	while (UNLIKELY(PTR_MISALIGNMENT(dstp) != 0 && n != 0))
	{
		*dstp = *srcp;
		dstp++;
		srcp++;
		n--;
	}

	dst = dstp;
	src = srcp;

	if (UNLIKELY(n == 0)) {
		return result;
	}

	// Copy word by word.
	dstpp = dst;
	srcpp = src;
	while (LIKELY(n >= sizeof(uintptr_t)))
	{
		*dstpp = *srcpp;
		dstpp++;
		srcpp++;
		n -= sizeof(uintptr_t);
	}

	dst = dstpp;
	src = srcpp;

	// Copy the end byte by byte if needed.
	if (LIKELY(n == 0)) {
		return result;
	}

CopyByteByByte:
	dstp = dst;
	srcp = src;

	while (n != 0)
	{
		*dstp = *srcp;
		dstp++;
		srcp++;
		n--;
	}

	return result;
}

void* memmove(void* dst, const void* src, size_t n)
{
	uintptr_t dstis = (uintptr_t) dst;
	uintptr_t srcis = (uintptr_t) src;
	uintptr_t dstie = dstis + n, srcie = srcis + n;

	// If dst and src don't overlap, fall back to memcpy.
	if (LIKELY(dstie <= srcis || srcie <= dstis)) {
		return memcpy(dst, src, n);
	}

	char* dstchr = dst;
	const char* srcchr = src;

	if (dstis <= srcis)
	{
		// Forward copy byte for byte.
		while (n--)
			*(dstchr++) = *(srcchr++);

		return dst;
	}
	else
	{
		// Reverse copy.
		dstchr += n;
		srcchr += n;
		while (n--)
			*(--dstchr) = *(--srcchr);

		return dst;
	}
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

#ifdef IS_32_BIT
#define WORD_ONES 0x01010101
#else
#define WORD_ONES 0x0101010101010101ULL
#endif

void* memset(void* dst, int c, size_t n)
{
	void* result = dst;

	uint8_t* dstp = dst;
	while (UNLIKELY((n < sizeof(uintptr_t) || PTR_MISALIGNMENT(dstp) != 0) && n != 0))
	{
		*dstp++ = c;
		n--;
	}

	dst = dstp;

	if (UNLIKELY(n == 0)) {
		return result;
	}

	uintptr_t* dstpp = dst;
	uintptr_t cw = c & 0xFF;
	cw = cw * WORD_ONES;

	while (LIKELY(n >= sizeof(uintptr_t)))
	{
		*dstpp = cw;
		dstpp++;
		n -= sizeof(uintptr_t);
	}

	dst = dstpp;

	if (LIKELY(n == 0)) {
		return result;
	}

	// Final byte for byte setting.
	dstp = dst;
	while (n--)
		*dstp++ = c;

	return result;
}

size_t strlen(const char* s)
{
#if defined KERNEL && defined DEBUG
	if (!s)
		KeCrash("strlen(NULL)");
#endif

#ifdef STRLEN_MAY_ACCESS_PAST_STRING
	size_t sz = 0;
	while (UNLIKELY(PTR_MISALIGNMENT(s) != 0 && *s != 0)) {
		s++;
		sz++;
	}

	if (UNLIKELY(*s == 0)) {
		return sz;
	}

	const uintptr_t* sp = (const uintptr_t*) s;
	const uintptr_t highbytes = WORD_ONES * 0x80;
	while (true)
	{
		uintptr_t check = *sp;
		if (((check - WORD_ONES) & ~check) & highbytes) {
			s = (const char*) sp;
			break;
		}

		// None of them are zero, skip altogether
		sp++;
		sz += sizeof(uintptr_t);
	}
#endif

	while (UNLIKELY(*s != 0)) {
		s++;
		sz++;
	}

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

int strncmp(const char *str1, const char *str2, size_t len)
{
	for (size_t i = 0; i < len; i++)
	{
		if (str1[i] != str2[i])
			return str1[i] < str2[i] ? -1 : 1;

		if (str1[i] == 0)
			return 0;
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

char* strstr(const char* haystack, const char* needle)
{
	for (size_t i = 0; haystack[i] != 0; i++)
	{
		if (strncmp(haystack + i, needle, strlen(needle)) == 0)
			return (char*) haystack + i;
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
