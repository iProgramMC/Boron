//
//  new_delete.cpp
//
//  Copyright (C) 2026 iProgramInCpp.
//  Created on 9/4/2026.
//
#include <boron.h>
#include <rtl/assert.h>

void* operator new(size_t Size)
{
	if (Size == 0) {
		DbgPrint("you should not be using allocations of size 0.");
		Size++;
	}
	
	void* Memory = ::OSAllocate(Size);
	
	if (!Memory) {
		DbgPrint("operator new() failed.");
		RtlAbort();
	}
	
	return Memory;
}

void* operator new[](size_t Size)
{
	if (Size == 0) {
		DbgPrint("you should not be using allocations of size 0.");
		Size++;
	}
	
	void* Memory = ::OSAllocate(Size);
	
	if (!Memory) {
		DbgPrint("operator new() failed.");
		RtlAbort();
	}
	
	return Memory;
}

void operator delete(void* Pointer)
{
	::OSFree(Pointer);
}

void operator delete(void* Pointer, UNUSED size_t Size)
{
	::OSFree(Pointer);
}

void operator delete[](void* Pointer)
{
	::OSFree(Pointer);
}

void operator delete[](void* Pointer, UNUSED size_t Size)
{
	::OSFree(Pointer);
}
