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

// NOTE: This is a non-standard entry format. Normally applications' entry points are no-return
// with no arguments. No OS, as far as I know, does this. However, because I am lazy enough not
// to want to write a crt0 that links with every object, I will instead treat the application's
// entry point as an int main(int argc, char** argc, char** envp).
//
// This doesn't come without caveats, however, if libboron launches a POSIX application (f.ex.
// compiled and linked with mlibc), it will actually call upon ld.so to interpret the program
// instead of doing it itself.  This allows ld.so to perform System V ABI-compliant stack setup
// which libboron.so completely ignores.
typedef int(*OSDLL_ENTRY_POINT)(int ArgumentCount, char** ArgumentArray, char** EnvironmentArray);

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

// Maps an ELF file into the target process' memory.
HIDDEN
BSTATUS OSDLLMapElfFile(
	PPEB Peb,
	HANDLE ProcessHandle,
	HANDLE FileHandle,
	const char* Name,
	OSDLL_ENTRY_POINT* OutEntryPoint,
	int FileKind
);

// Opens a file by name, scanning the PATH environment variable.
HIDDEN
BSTATUS OSDLLOpenFileByName(PHANDLE Handle, const char* FileName, bool IsLibrary);
