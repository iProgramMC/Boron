#include <boron.h>
#include <string.h>
#include "misc.h"

static const char* FileName = "\\InitRoot\\ext2fs.sys";

void RunTest4()
{
	// Now, test4 is supposed to test the following:
	//
	// - Open a file
	// - Map five pages as read/write without CoW
	// - Clear two pages (tests upgradeability and direct writing to cache)
	// - Unmapping the file, and closing
	//
	// Now, note that currently, even if a file is read only it is still
	// mappable read/write.  I will fix that. (TODO)
	//
	// As of 11.8.25, when a file is unmapped, the modified pages end up
	// in the modified page list and the MPW gets triggered immediately.
	
	BSTATUS Status;
	HANDLE Handle;
	OBJECT_ATTRIBUTES Attributes;
	void* AddressV;
	uint8_t* Address;
	
	memset(&Attributes, 0, sizeof Attributes);
	
	Attributes.ObjectName = FileName;
	Attributes.ObjectNameLength = strlen(FileName);
	
	Status = OSOpenFile(&Handle, &Attributes);
	CHECK_STATUS(4);
	
	AddressV = NULL;
	Status = OSMapViewOfObject(
		CURRENT_PROCESS_HANDLE,
		Handle,
		&AddressV,
		5 * PAGE_SIZE,
		MEM_COMMIT,
		0,
		PAGE_READ | PAGE_WRITE
	);
	CHECK_STATUS(4);
	
	// Write to the first two pages.
	Address = AddressV;
	*((uint32_t*)Address) = 0xCAFEBABE;
	*((uint32_t*)&Address[PAGE_SIZE]) = 0xDEADBEEF;
	
	// Now unmap and close the file.
	Status = OSFreeVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		AddressV,
		5 * PAGE_SIZE,
		MEM_RELEASE
	);
	CHECK_STATUS(4);
	
	Status = OSClose(Handle);
	CHECK_STATUS(4);
	
	// Now the modified page writer shenanigans should start...
	DbgPrint("Okay, MPW test prepared. Now wait.");
}
