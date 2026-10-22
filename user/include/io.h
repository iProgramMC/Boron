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

HANDLE OSGetStandardInput();

HANDLE OSGetStandardOutput();

HANDLE OSGetStandardError();

BSTATUS OSCreateFile(
	PHANDLE OutFileHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock,
	uint32_t FileAttributes,
	uint64_t AllocationSize,
	uint32_t CreateDisposition,
	uint32_t CreateOptions,
	const char* TargetName,
	size_t TargetNameLength
);

BSTATUS OSOpenFile(
	PHANDLE OutFileHandle,
	POBJECT_ATTRIBUTES ObjectAttributes
);

BSTATUS OSOpenFileEx(
	PHANDLE OutFileHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	uint32_t FileType
);

#ifdef __cplusplus
}
#endif
