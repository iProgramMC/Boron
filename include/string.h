// Boron - String operations
#ifndef NS64_STRING_H
#define NS64_STRING_H

#include <stdint.h>

void* memcpy(void* dst, const void* src, size_t n);
void* memquadcpy(uint64_t* dst, const uint64_t* src, size_t n);
void* memset(void* dst, int c, size_t n);
size_t strlen(const char * s);
char* strcpy(char* s, const char * d);
char* strcat(char* s, const char * src);
int memcmp(const void* s1, const void* s2, size_t n);
int strcmp(const char* s1, const char* s2);

#endif//NS64_STRING_H