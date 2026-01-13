#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Prints a formatted string to standard output.
size_t OSPrintf(const char* format, ...);

// Prints a formatted string to standard output or standard error.
size_t OSFPrintf(int FileIndex, const char* Format, ...);

#ifdef __cplusplus
}
#endif
