#include <main.h>

void* memcpy(void* dst, const void* src, size_t n)
{
	void* dst2 = dst;
	ASM("rep movsb":"+c"(n),"+D"(dst),"+S"(src)::"memory");
	return dst2;
}

void* memquadcpy(uint64_t* dst, const uint64_t* src, size_t n)
{
	void* dst2 = dst;
	ASM("rep movsq":"+c"(n),"+D"(dst),"+S"(src)::"memory");
	return dst2;
}

void* memset(void* dst, int c, size_t n)
{
	void* dst2 = dst;
	ASM("rep stosb":"+c"(n),"+D"(dst):"a"(c):"memory");
	return dst2;
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
