#pragma once

#include <rtl/elf.h>

#ifdef DEBUG2
#define LOADER_DEBUG
#endif

#ifdef LOADER_DEBUG
#define LdrDbgPrint(...) DbgPrint(__VA_ARGS__)
#else
#define LdrDbgPrint(...) do {} while (0)
#endif

enum
{
	FILE_KIND_MAIN_EXECUTABLE,
	FILE_KIND_DYNAMIC_LIBRARY,
	FILE_KIND_INTERPRETER,
};

typedef struct
{
	LIST_ENTRY ListEntry;
	char Name[32];
	
	int FileKind;
	uintptr_t ImageBase;
	PELF_DYNAMIC_ITEM DynamicTable;
	ELF_DYNAMIC_INFO DynamicInfo;
}
LOADED_IMAGE, *PLOADED_IMAGE;

typedef struct
{
	LIST_ENTRY ListEntry;
	char PathName[];
}
DLL_LOAD_QUEUE_ITEM, *PDLL_LOAD_QUEUE_ITEM;

// Gets the image base of libboron.so.
uintptr_t RtlGetImageBase();

typedef int(*ELF_ENTRY_POINT2)();

// Maps an ELF file into the target process' memory.
HIDDEN
BSTATUS OSDLLMapElfFile(
	PPEB Peb,
	HANDLE ProcessHandle,
	HANDLE FileHandle,
	const char* Name,
	ELF_ENTRY_POINT2* OutEntryPoint,
	int FileKind
);

// Opens a file by name, scanning the PATH environment variable.
HIDDEN
BSTATUS OSDLLOpenFileByName(PHANDLE Handle, const char* FileName);
