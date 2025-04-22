#pragma once

#include <main.h>
#include <status.h>

#define CURRENT_PROCESS_HANDLE ((HANDLE) 0xFFFFFFFF)
#define CURRENT_THREAD_HANDLE  ((HANDLE) 0xFFFFFFFE)

#define TIMEOUT_INFINITE (0x7FFFFFFF)

typedef NO_RETURN void(*PKTHREAD_START)(void* Context);

typedef uintptr_t HANDLE, *PHANDLE;

typedef struct _OBJECT_ATTRIBUTES
{
	HANDLE RootDirectory;
	const char* ObjectName;
	size_t ObjectNameLength;
	int OpenFlags;
}
OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;
