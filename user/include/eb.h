#pragma once

#include "handle.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TEB
{
	PPEB Peb;
	
	HANDLE SpareHandle;
	
	// More to define here such as TLS
}
TEB, *PTEB;

// Get a pointer to the current TEB.
PTEB OSDLLGetCurrentTeb();

// Get a pointer to the current PEB.
PPEB OSDLLGetCurrentPeb();

// Gets the working directory of the process.
HANDLE OSGetCurrentDirectory();

// Gets the working directory of the process, in path form.
//
// Note that if the current directory's path is longer than the
// provided buffer, STATUS_BUFFER_OVERFLOW is returned, but a valid
// null-terminated string is returned, just truncated.
BSTATUS OSGetCurrentDirectoryPath(char* OutBuffer, size_t BufferSize);

// Sets the current working directory of the current thread.
//
// Note that this path MUST be an absolute path.  Relative
// paths do not work.
BSTATUS OSSetCurrentDirectory(const char* NewDirectoryPath);

#ifdef __cplusplus
}
#endif
