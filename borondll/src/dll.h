#pragma once

typedef struct
{
	LIST_ENTRY ListEntry;
	
	char Name[32];
	
	uintptr_t ImageBase;
	
	char* StringTable;
	char* DynamicTable;
}
LOADED_IMAGE, *PLOADED_IMAGE;

typedef struct
{
	LIST_ENTRY ListEntry;
	
	bool LoadedPathName;
	
	union
	{
		uintptr_t PathNameOffsetInStrTab;
		const char* PathName;
	};
}
DLL_LOAD_QUEUE_ITEM, *PDLL_LOAD_QUEUE_ITEM;