#include <boron.h>
#include <string.h>
#include <rtl/elf.h>
#include <rtl/assert.h>
#include "pebteb.h"
#include "dll.h"

#ifdef IS_64_BIT
#define USER_SPACE_END 0x0000800000000000
#else
#define USER_SPACE_END 0x80000000
#endif

static BSTATUS OSDLLOpenSelf(PHANDLE FileHandle)
{
	extern char _DYNAMIC[];
	return OSGetMappedFileHandle(FileHandle, CURRENT_PROCESS_HANDLE, (uintptr_t) _DYNAMIC);
}

static size_t OSDLLCalculatePebSize(const char* ImageName, const char* CommandLine)
{
	size_t PebSize = sizeof(PEB);
	PebSize += strlen(ImageName) + 8 + sizeof(uintptr_t);
	PebSize += strlen(CommandLine) + 8 + sizeof(uintptr_t);
	PebSize = (PebSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);	
	return PebSize;
}

static BSTATUS OSDLLCreatePebForProcess(PPEB* OutPeb, size_t* OutPebSize, const char* ImageName, const char* CommandLine)
{
	size_t PebSize = OSDLLCalculatePebSize(ImageName, CommandLine);
	
	// Create the PEB
	PPEB Peb = OSAllocate(PebSize);
	if (!Peb)
		return STATUS_INSUFFICIENT_MEMORY;
	
	// Fill in the PEB.
	memset(Peb, 0, sizeof(PEB));
	
	char* AfterPeb = (char*) &Peb[1];
	Peb->ImageName = AfterPeb;
	Peb->ImageNameSize = strlen(ImageName);
	AfterPeb += Peb->ImageNameSize + 1;
	
	AfterPeb = (char*)(((uintptr_t)AfterPeb + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1));
	Peb->CommandLine = AfterPeb;
	Peb->CommandLineSize = strlen(CommandLine);
	
	strcpy(Peb->ImageName, ImageName);
	strcpy(Peb->CommandLine, CommandLine);
	
	*OutPeb = Peb;
	*OutPebSize = PebSize;
	return STATUS_SUCCESS;
}

static BSTATUS OSDLLPreparePebForProcess(
	HANDLE ProcessHandle,
	PPEB Peb,
	size_t PebSize,
	void** PebPtrOut,
	bool InheritHandles
)
{
	BSTATUS Status;
	
	// Allocate and copy the PEB into the executable's memory.
	void* PebPtr = NULL;
	size_t RegionSize = PebSize;
	Status = OSAllocateVirtualMemory(
		ProcessHandle,
		&PebPtr,
		&RegionSize,
		MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN,
		PAGE_READ | PAGE_WRITE
	);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Cannot map PEB: %s (%d)", RtlGetStatusString(Status), Status);
		return Status;
	}
	
	// Update all the pointers to point to the correct place.
	Peb->ImageName   = (char*)((uintptr_t)Peb->ImageName   + (uintptr_t)PebPtr - (uintptr_t)Peb);
	Peb->CommandLine = (char*)((uintptr_t)Peb->CommandLine + (uintptr_t)PebPtr - (uintptr_t)Peb);
	Peb->PebFreeSize = RegionSize;
	
	// Duplicate the standard input/output handles, unless InheritHandles is true, in which case just
	// expect them to already be given to the process.
	PPEB CurrentPeb = OSDLLGetCurrentPeb();
	if (InheritHandles)
	{
		for (int i = 0; i < 3; i++)
			Peb->StandardIO[i] = CurrentPeb->StandardIO[i];
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			if (!CurrentPeb->StandardIO[i])
				continue;
			
			Status = OSDuplicateHandle(CurrentPeb->StandardIO[i], ProcessHandle, &Peb->StandardIO[i], 0);
			if (FAILED(Status))
				return Status;
		}
	}
	
	// TODO: Copy the environment variables from our PEB into
	// the PEB of the receiving process
	
	
	
	Status = OSSetPebProcess(ProcessHandle, PebPtr);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Failed to OSSetPebProcess: %s (%d)", RtlGetStatusString(Status), Status);
		return Status;
	}
	
	// Now copy the PEB into the destination application.
	Status = OSWriteVirtualMemory(ProcessHandle, PebPtr, Peb, PebSize);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Failed to OSWriteVirtualMemory: %s (%d)", RtlGetStatusString(Status), Status);
		return Status;
	}
	
	*PebPtrOut = PebPtr;
	return Status;
}

static BSTATUS OSDLLCreateMainThread(
	HANDLE ProcessHandle,
	PHANDLE OutMainThreadHandle,
	void* EntryPoint,
	void* Peb,
	bool CreateSuspended
)
{
	return OSCreateThread(
		OutMainThreadHandle,
		ProcessHandle,
		NULL,
		EntryPoint,
		Peb,
		CreateSuspended
	);
}

BSTATUS OSCreateProcess(
	PHANDLE OutHandle,
	PHANDLE OutMainThreadHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	bool InheritHandles,
	bool CreateSuspended,
	const char* ImageName,
	const char* CommandLine
)
{
	// Create the process
	HANDLE ProcessHandle;
	HANDLE FileHandle;
	void *PebPtr = NULL;
	ELF_ENTRY_POINT2 EntryPoint = NULL;
	
	BSTATUS Status = OSCreateProcessInternal(
		OutHandle,
		ObjectAttributes,
		CURRENT_PROCESS_HANDLE,
		InheritHandles
	);
	if (FAILED(Status))
		return Status;
	
	ProcessHandle = *OutHandle;
	
	// Create the PEB for this process.
	PPEB Peb = NULL;
	size_t PebSize = 0;
	Status = OSDLLCreatePebForProcess(&Peb, &PebSize, ImageName, CommandLine);
	if (FAILED(Status))
	{
	Fail:
		if (Peb) OSFree(Peb);
		OSClose(ProcessHandle);
		return Status;
	}
	
	// Open the main image.
	Status = OSDLLOpenFileByName(&FileHandle, ImageName);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Failed to open %s. %s (%d)", ImageName, RtlGetStatusString(Status), Status);
		goto Fail;
	}
	
	// Now map the main image inside.
	LdrDbgPrint("OSCreateProcess: Mapping main image %s.", ImageName);
	Status = OSDLLMapElfFile(Peb, ProcessHandle, FileHandle, ImageName, &EntryPoint, true);
	OSClose(FileHandle);
	
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Failed to map the %s executable. %s (%d)", ImageName, RtlGetStatusString(Status), Status);
		goto Fail;
	}
	
	ASSERT(Peb->Loader.MappedImage);
	
	if (Peb->Loader.Interpreter)
	{
		// There is an interpreter.
		const char* Interpreter = Peb->Loader.Interpreter;
		
		HANDLE InterpreterFileHandle = HANDLE_NONE;
		if (strcmp(Interpreter, "libboron.so") == 0)
		{
			LdrDbgPrint("OSDLL: The executable demands libboron.so, so map that.", Interpreter);
			
			// This is libboron.so, so no meaning in scanning for anything
			// because we can just open ourselves.
			Status = OSDLLOpenSelf(&InterpreterFileHandle);
		}
		else
		{
			LdrDbgPrint("OSDLL: Opening interpreter %s.", Interpreter);
			Status = OSDLLOpenFileByName(&InterpreterFileHandle, Interpreter);
		}
		
		//Status = OSDLLMapSelfIntoProcess(ProcessHandle, InterpreterFileHandle, PebSize, &EntryPoint);
		
		LdrDbgPrint("OSCreateProcess: Mapping interpreter %s.", Interpreter);
		Status = OSDLLMapElfFile(Peb, ProcessHandle, InterpreterFileHandle, Interpreter, &EntryPoint, false);
		OSClose(InterpreterFileHandle);
		
		if (FAILED(Status))
		{
			DbgPrint("OSDLL: Failed to map the interpreter %s. %s (%d)", Interpreter, RtlGetStatusString(Status), Status);
			goto Fail;
		}
	}
	
	Status = OSDLLPreparePebForProcess(ProcessHandle, Peb, PebSize, &PebPtr, InheritHandles);
	if (FAILED(Status))
		goto Fail;
	
	OSFree(Peb);
	Peb = NULL;
	
	LdrDbgPrint("OSDLL: Entry Point: %p", EntryPoint);
	Status = OSDLLCreateMainThread(ProcessHandle, OutMainThreadHandle, EntryPoint, PebPtr, CreateSuspended);
	if (FAILED(Status))
		goto Fail;
	
	return STATUS_SUCCESS;
}
