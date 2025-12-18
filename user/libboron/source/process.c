#include <boron.h>
#include <string.h>
#include <rtl/elf.h>
#include <rtl/assert.h>
#include <rtl/cmdline.h>
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

static size_t OSDLLCalculatePebSize(const char* ImageName, const char* CommandLine, const char* Environment)
{
	size_t PebSize = sizeof(PEB);
	PebSize += strlen(ImageName) + 8 + sizeof(uintptr_t);
	PebSize += RtlEnvironmentLength(CommandLine) + 8 + sizeof(uintptr_t);
	PebSize += RtlEnvironmentLength(Environment) + 8 + sizeof(uintptr_t);
	PebSize = (PebSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);	
	return PebSize;
}

static BSTATUS OSDLLCreatePebForProcess(PPEB* OutPeb, size_t* OutPebSize, const char* ImageName, const char* CommandLine, const char* Environment)
{
	size_t PebSize = OSDLLCalculatePebSize(ImageName, CommandLine, Environment);
	
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
	Peb->CommandLineSize = RtlEnvironmentLength(CommandLine);
	AfterPeb += Peb->CommandLineSize + 1;
	
	AfterPeb = (char*)(((uintptr_t)AfterPeb + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1));
	Peb->Environment = AfterPeb;
	Peb->EnvironmentSize = RtlEnvironmentLength(Environment);
	
	strcpy(Peb->ImageName, ImageName);
	memcpy(Peb->CommandLine, CommandLine, Peb->CommandLineSize);
	memcpy(Peb->Environment, Environment, Peb->EnvironmentSize);
	
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
	Peb->Environment = (char*)((uintptr_t)Peb->Environment + (uintptr_t)PebPtr - (uintptr_t)Peb);
	Peb->PebFreeSize = RegionSize;
	
	// Duplicate the standard input/output handles, unless InheritHandles is true, in which case just
	// expect them to already be given to the process.
	PPEB CurrentPeb = OSDLLGetCurrentPeb();
	if (InheritHandles)
	{
		// N.B.  These handles shouldn't be opened with OB_FLAG_NO_INHERIT!  Otherwise, the child
		// process will try to mess with invalid handles, or even worse.
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
	int ProcessFlags,
	const char* ImageName,
	const char* CommandLine,
	const char* Environment
)
{
	// Pointers/resources freed on failure
	HANDLE ProcessHandle = HANDLE_NONE;
	PPEB Peb = NULL;
	void *PebPtr = NULL;
	char *AllocatedCmdLine = NULL;
	
	BSTATUS Status = STATUS_SUCCESS;
	HANDLE FileHandle;
	OSDLL_ENTRY_POINT EntryPoint = NULL;
	
	if (!CommandLine)
		CommandLine = "";
	
	// If the command line is in regular string form, convert it to environment
	// description format.
	if (~ProcessFlags & OS_PROCESS_CMDLINE_PARSED)
	{
		Status = RtlCommandLineStringToDescription(CommandLine, &AllocatedCmdLine);
		if (FAILED(Status))
		{
			DbgPrint("OSDLL: Failed to parse command line for process. %s (%d)", RtlGetStatusString(Status), Status);
			goto Fail;
		}
		
		CommandLine = AllocatedCmdLine;
	}
	
	// Create the process.
	Status = OSCreateProcessInternal(
		OutHandle,
		ObjectAttributes,
		CURRENT_PROCESS_HANDLE,
		ProcessFlags & OS_PROCESS_INHERIT_HANDLES,
		ProcessFlags & OS_PROCESS_DEEP_CLONE_HANDLES
	);
	if (FAILED(Status))
		goto Fail;
	
	ProcessHandle = *OutHandle;
	
	// If the environment wasn't specified, copy the one from our process.
	if (!Environment)
		Environment = OSDLLGetCurrentPeb()->Environment;
	
	// Create the PEB for this process.
	size_t PebSize = 0;
	Status = OSDLLCreatePebForProcess(&Peb, &PebSize, ImageName, CommandLine, Environment);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Failed to create PEB for process. %s (%d)", RtlGetStatusString(Status), Status);
		goto Fail;
	}
	
	// Open the main image.
	Status = OSDLLOpenFileByName(&FileHandle, ImageName, false);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Failed to open %s. %s (%d)", ImageName, RtlGetStatusString(Status), Status);
		goto Fail;
	}
	
	// Now map the main image inside.
	LdrDbgPrint("OSCreateProcess: Mapping main image %s.", ImageName);
	Status = OSDLLMapElfFile(Peb, ProcessHandle, FileHandle, ImageName, &EntryPoint, FILE_KIND_MAIN_EXECUTABLE);
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
		const size_t MaxSize = 32;
		const char* InterpreterSrc = Peb->Loader.Interpreter;
		char* InterpreterCopy = OSAllocate(MaxSize);
		if (!InterpreterCopy)
		{
			DbgPrint("OSDLL: Failed to allocate memory for interpreter string.");
			Status = STATUS_INSUFFICIENT_MEMORY;
			goto Fail;
		}
		
		Status = OSReadVirtualMemory(ProcessHandle, InterpreterCopy, InterpreterSrc, MaxSize);
		if (FAILED(Status))
		{
			DbgPrint("OSDLL: Failed to copy the interpreter string into memory.");
			OSFree(InterpreterCopy);
			goto Fail;
		}
		
		InterpreterCopy[MaxSize - 1] = 0;
		
		HANDLE InterpreterFileHandle = HANDLE_NONE;
		if (strcmp(InterpreterCopy, "libboron.so") == 0)
		{
			LdrDbgPrint("OSDLL: The executable demands libboron.so, so map that.", InterpreterCopy);
			
			// This is libboron.so, so no meaning in scanning for anything
			// because we can just open ourselves.
			Status = OSDLLOpenSelf(&InterpreterFileHandle);
		}
		else
		{
			LdrDbgPrint("OSDLL: Opening interpreter %s.", InterpreterCopy);
			Status = OSDLLOpenFileByName(&InterpreterFileHandle, InterpreterCopy, true);
		}
		
		LdrDbgPrint("OSCreateProcess: Mapping interpreter %s.", InterpreterCopy);
		
		if (FAILED(Status))
		{
			OSFree(InterpreterCopy);
			goto Fail;
		}
		
		Status = OSDLLMapElfFile(Peb, ProcessHandle, InterpreterFileHandle, InterpreterCopy, &EntryPoint, FILE_KIND_INTERPRETER);
		OSClose(InterpreterFileHandle);
		
		if (FAILED(Status))
		{
			DbgPrint("OSDLL: Failed to map the interpreter %s. %s (%d)", InterpreterCopy, RtlGetStatusString(Status), Status);
			OSFree(InterpreterCopy);
			goto Fail;
		}
		
		OSFree(InterpreterCopy);
	}
	
	Status = OSDLLPreparePebForProcess(
		ProcessHandle,
		Peb,
		PebSize,
		&PebPtr,
		ProcessFlags & (OS_PROCESS_INHERIT_HANDLES | OS_PROCESS_DEEP_CLONE_HANDLES)
	);
	if (FAILED(Status))
		goto Fail;
	
	// Free these resources earlier than necessary.
	OSFree(Peb);
	OSFree(AllocatedCmdLine);
	Peb = NULL;
	AllocatedCmdLine = NULL;
	
	LdrDbgPrint("OSDLL: Entry Point: %p", EntryPoint);
	Status = OSDLLCreateMainThread(
		ProcessHandle,
		OutMainThreadHandle,
		EntryPoint,
		PebPtr,
		ProcessFlags & OS_PROCESS_CREATE_SUSPENDED
	);
	if (FAILED(Status))
		goto Fail;
	
	return STATUS_SUCCESS;
	
Fail:
	if (Peb) OSFree(Peb);
	if (AllocatedCmdLine) OSFree(AllocatedCmdLine);
	if (ProcessHandle) OSClose(ProcessHandle);
	return Status;
}
