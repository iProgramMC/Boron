#include "testfmk.h"

void Test4ListDirectory()
{
	HANDLE Handle;
	BSTATUS Status;
	
	OBJECT_ATTRIBUTES Attributes;
	OSInitializeObjectAttributes(&Attributes);
	OSSetNameObjectAttributes(&Attributes, "/usr/include");
	
	// open the directory to list it.
	Status = OSOpenFile(&Handle, &Attributes);
	
	TestAssert(SUCCEEDED(Status));
	TestAssert(Handle != HANDLE_NONE);
	
	IO_DIRECTORY_ENTRY Entry;
	while (true)
	{
		IO_STATUS_BLOCK Iosb;
		Status = OSReadDirectoryEntries(&Iosb, Handle, 1, &Entry);
		
		TestAssert(IOSUCCEEDED(Status));
		TestAssert(IOSUCCEEDED(Iosb.Status));
		
		if (Iosb.Status == STATUS_END_OF_FILE)
			break;
		
		//DbgPrint("File name read: '%s'.", Entry.Name);
	}
	
	Status = OSClose(Handle);
	TestAssert(SUCCEEDED(Status));
}
