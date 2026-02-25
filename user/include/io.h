#pragma once

#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

// Prints a formatted string to standard output.
size_t OSPrintf(const char* format, ...);

// Prints a formatted string to standard output or standard error.
size_t OSFPrintf(int FileIndex, const char* Format, ...);

#define OSInitializeObjectAttributes(Attributes) \
	memset((Attributes), 0, sizeof(OBJECT_ATTRIBUTES));

#define OSSetNameObjectAttributes(Attributes, Name) do {               \
	(Attributes)->ObjectName = (Name);                                 \
	(Attributes)->ObjectNameLength = strlen((Attributes)->ObjectName); \
} while (0)

#ifdef __cplusplus
}
#endif
