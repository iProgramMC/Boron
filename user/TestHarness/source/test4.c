#include "testfmk.h"

void Test4CreateFile()
{
	HANDLE Handle, RootDirHandle;
	BSTATUS Status;
	
	OBJECT_ATTRIBUTES Attributes;
	OSInitializeObjectAttributes(&Attributes);
	OSSetNameObjectAttributes(&Attributes, "/");
	
	// open the root directory to create a file there.
	Status = OSOpenFile(&RootDirHandle, &Attributes);
	
	TestAssert(SUCCEEDED(Status));
	TestAssert(RootDirHandle != HANDLE_NONE);
	
	// try creating a file in there.  as of 25/02, this points
	// to a tmpfs directory.
	const char* FileName = "Test File";
	Status = OSCreateFile(&Handle, RootDirHandle, FileName, strlen(FileName));
	TestAssert(SUCCEEDED(Status));
	TestAssert(Handle != HANDLE_NONE);
	
	Status = OSClose(Handle);
	TestAssert(SUCCEEDED(Status));
	Status = OSClose(RootDirHandle);
	TestAssert(SUCCEEDED(Status));
}
