#pragma once

#include <rtl/elf.h>

typedef struct
{
	LIST_ENTRY ListEntry;
	char Name[32];
	
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
