/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/initproc.c
	
Abstract:
	This module implements the code which initializes the
	initial user process in the Boron operating system.
	
	This initial process is "init.exe", loaded by libboron.so.
	
Author:
	iProgramInCpp - 16 April 2025
***/
#include "psp.h"
#include <io.h>
#include <rtl/elf.h>

// TODO: make these changeable

// path to libboron.so. temporary
const char* PspBoronDllFileName = "/InitRoot/libboron.so";

// path to init.exe and command line. temporary
const char* PspInitialProcessFileName = "/InitRoot/init.exe";
const char* PspInitialProcessCommandLine = "";

// environment. temporary.
const char* PspInitialProcessEnvironment =
	"PATH=/Mount/Nvme0Disk1Part0:/InitRoot:/\0"
	"SOMETHING=Something here\0"
	"\0";

// TODO THIS IS A PLACEHOLDER
#include <limreq.h>

bool PsShouldStartInitialProcess()
{
	return !StringContainsCaseInsensitive(KeGetBootCommandLine(), "/noinit");
}

// Unlike strlen(), this counts the amount of characters in an environment
// variable description.  Environment variables are separated with "\0" and
// the final environment variable finishes with two "\0" characters.
static size_t PspEnvironmentLength(const char* Environ)
{
	size_t Length = 2;
	while (Environ[0] != '\0' && Environ[1] != '\0') {
		Environ++;
		Length++;
	}
	
	return Length;
}

// TODO: Share a lot of this code with Ldr.

static PELF_PROGRAM_HEADER PspLdrFindDynamicPhdr(
	uint8_t* ProgramHeaders,
	size_t ProgramHeaderSize,
	size_t ProgramHeaderCount)
{
	for (size_t i = 0; i < ProgramHeaderCount; i++)
	{
		PELF_PROGRAM_HEADER ProgramHeader = (void*) (ProgramHeaders + i * ProgramHeaderSize);
		
		if (ProgramHeader->Type == PROG_DYNAMIC)
			return ProgramHeader;
	}
	
	return NULL;
}

NO_RETURN
void PsStartInitialProcess(UNUSED void* ContextUnused)
{
	BSTATUS Status;
	HANDLE ProcessHandle;
	HANDLE FileHandle;
	OBJECT_ATTRIBUTES FileAttributes;
	ELF_HEADER ElfHeader;
	const char* Func;
	
	// TODO: Make these changeable!
	const char* BoronDllPath = PspBoronDllFileName;
	
	const char* ImageName = PspInitialProcessFileName;
	const char* CommandLine = PspInitialProcessCommandLine;
	const char* Environment = PspInitialProcessEnvironment;
	
	Func = __func__;
	
	Status = OSCreateProcess(
		&ProcessHandle,
		NULL,
		CURRENT_PROCESS_HANDLE,
		false
	);
	
	if (FAILED(Status))
		KeCrash("%s: Failed to create initial process: %d (%s)", Status, RtlGetStatusString(Status));
	
	FileAttributes.RootDirectory = HANDLE_NONE;
	FileAttributes.ObjectName = BoronDllPath;
	FileAttributes.ObjectNameLength = strlen(BoronDllPath);
	FileAttributes.OpenFlags = 0;
	
	Status = OSOpenFile(
		&FileHandle,
		&FileAttributes
	);
	
	if (FAILED(Status))
		KeCrash("%s: Failed to open %s: %d (%s)", Func, BoronDllPath, Status, RtlGetStatusString(Status));
	
	// The file has been opened.
	uint64_t FileSize = 0;
	Status = OSGetLengthFile(FileHandle, &FileSize);
	if (FAILED(Status))
		KeCrash("%s: Failed to get length of file %s: %d (%s)", Func, BoronDllPath, Status, RtlGetStatusString(Status));
	
	// XXX: Arbitrary
	if (FileSize > 0x800000)
		KeCrash("%s: %s is just too big", Func, BoronDllPath);
	
	// There is no need to complicate ourselves at all.  We will simply map the
	// entire file once, then map each page individually where needed, and unmap
	// the temporary mapping.
	uint8_t* ElfMappingBase = NULL;
	size_t RegionSize = FileSize;
	
	Status = OSMapViewOfObject(
		CURRENT_PROCESS_HANDLE,
		FileHandle,
		(void**) &ElfMappingBase,
		RegionSize,
		0,
		0,
		PAGE_READ
	);
	
	if (FAILED(Status))
		KeCrash("%s: Could not map %s into memory: %d (%s)", Func, BoronDllPath, Status, RtlGetStatusString(Status));
	
	PELF_HEADER PElfHeader = (PELF_HEADER) ElfMappingBase;
	memcpy(&ElfHeader, PElfHeader, sizeof(ELF_HEADER));
	
	// TODO: Check if this is a valid ELF header.
	
	// Read program header information.
	uint8_t* ProgramHeaders = ElfMappingBase + ElfHeader.ProgramHeadersOffset;
	
	uintptr_t BoronDllBase = 0;
	
	uintptr_t FirstAddr, LargestAddr;
	FirstAddr = (uintptr_t) ~0ULL;
	LargestAddr = 0;
	
	// Find the dynamic program header;
	PELF_PROGRAM_HEADER DynamicPhdr = PspLdrFindDynamicPhdr(
		ProgramHeaders,
		ElfHeader.ProgramHeaderSize,
		ElfHeader.ProgramHeaderCount
	);
	
	if (!DynamicPhdr)
		KeCrash("%s: Error, cannot find dynamic program header in libboron.so", Func);
	
	// Now map each of them in.
	for (int i = 0; i < ElfHeader.ProgramHeaderCount; i++)
	{
		PELF_PROGRAM_HEADER ProgramHeader = (void*) (ProgramHeaders + i * ElfHeader.ProgramHeaderSize);
		
		if (FirstAddr > ProgramHeader->VirtualAddress)
			FirstAddr = ProgramHeader->VirtualAddress;
		if (LargestAddr < ProgramHeader->VirtualAddress + ProgramHeader->SizeInMemory)
			LargestAddr = ProgramHeader->VirtualAddress + ProgramHeader->SizeInMemory;
	}
	
	if (FirstAddr > LargestAddr)
		KeCrash("%s: Error, first address %p > largest address %p", Func, FirstAddr, LargestAddr);
	
	uintptr_t Size = LargestAddr - FirstAddr;
	Size = (Size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	
	// Now, the PEB will occupy however many pages, so we will need to cut those out too.
	size_t PebSize = sizeof(PEB);
	
	// and the size of the command line and image name
	PebSize += strlen(ImageName) + 8 + sizeof(uintptr_t);
	PebSize += strlen(CommandLine) + 8 + sizeof(uintptr_t);
	PebSize += PspEnvironmentLength(Environment) + 8 + sizeof(uintptr_t);
	
	// Now round it up to a page size
	PebSize = (PebSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	
	// TODO: not sure why I have to subtract 0x1000
	BoronDllBase = MM_USER_SPACE_END + 1 - PebSize - Size - 0x1000;
	
	bool IsDynamicLoaded = false;
	uintptr_t BoronDllBaseOld = BoronDllBase;
	uintptr_t DynamicBaseAddress = BoronDllBase + DynamicPhdr->VirtualAddress;
	
	// Start mapping in the program headers.
	for (int i = 0; i < ElfHeader.ProgramHeaderCount; i++)
	{
		PELF_PROGRAM_HEADER ProgramHeader = (void*) (ProgramHeaders + i * ElfHeader.ProgramHeaderSize);
		
		if (ProgramHeader->SizeInMemory == 0)
			continue;
		
		if (ProgramHeader->Type != PROG_LOAD)
			continue;
		
		void* BaseAddress = (void*)(BoronDllBase + ProgramHeader->VirtualAddress);
		
		if ((uintptr_t) BaseAddress <= (uintptr_t) DynamicBaseAddress &&
			(uintptr_t) BaseAddress + ProgramHeader->SizeInMemory >= (uintptr_t) DynamicBaseAddress + DynamicPhdr->SizeInMemory)
			IsDynamicLoaded = true;
		
		int Protection = 0, Flags = MEM_COW;
		if (ProgramHeader->Flags & ELF_PHDR_EXEC)  Protection |= PAGE_EXECUTE;
		if (ProgramHeader->Flags & ELF_PHDR_READ)  Protection |= PAGE_READ;
		
		BSTATUS Status = OSMapViewOfObject(
			ProcessHandle,
			FileHandle,
			&BaseAddress,
			ProgramHeader->SizeInMemory,
			Flags,
			ProgramHeader->Offset,
			Protection
		);

		if (FAILED(Status))
		{
			KeCrash(
				"%s: Failed to map ELF phdr in: %d (%s)\n"
				"ProgramHeader->SizeInMemory: %zu\n"
				"ProgramHeader->VirtualAddress: %p\n"
				"ProgramHeader->Offset: %p\n"
				"BaseAddress: %p\n",
				Func,
				Status,
				RtlGetStatusString(Status),
				ProgramHeader->SizeInMemory,
				ProgramHeader->VirtualAddress,
				ProgramHeader->Offset,
				BaseAddress
			);
		}
	}
	
	// Program headers have been mapped.
	BoronDllBase = BoronDllBaseOld;
	
	// This assumes that the dynamic table is entirely mapped.
	if (!IsDynamicLoaded)
		KeCrash("%s: There is no LOAD segment that maps the DYNAMIC table.", Func);
	
	// Make sure to unmap from the system address space.
	Status = OSFreeVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		ElfMappingBase,
		RegionSize,
		MEM_RELEASE
	);
	if (FAILED(Status))
		KeCrash("%s: Failed to free initial mapping of ELF: %d (%s)\n%p %zu", Func, Status, RtlGetStatusString(Status), ElfMappingBase, RegionSize);
	
	ElfMappingBase = NULL;
	
	OSClose(FileHandle);
	
	// Now, map the PEB into memory and fill it in.
	void* PebPtr = NULL;
	RegionSize = PebSize;
	Status = OSAllocateVirtualMemory(ProcessHandle, &PebPtr, &RegionSize, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_READ | PAGE_WRITE);
	if (FAILED(Status))
		KeCrash("%s: Failed to allocate PEB: %d (%s)", Func, Status, RtlGetStatusString(Status));
	
	// Attach to this process so that we can write to the PEB.
	PEPROCESS Process = NULL;
	Status = ExReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, (void**) &Process);
	ASSERT(SUCCEEDED(Status));
	
	PEPROCESS OldAttached = PsSetAttachedProcess(Process);
	
	PPEB PPeb = PebPtr;
	PPeb->PebFreeSize = RegionSize;
	
	// Copy the image name.
	char* AfterPeb = (char*) PebPtr + sizeof(PEB);
	PPeb->ImageName = AfterPeb;
	PPeb->ImageNameSize = strlen(ImageName);
	strcpy(AfterPeb, ImageName);
	
	AfterPeb += PPeb->ImageNameSize + 1;
	
	// Align to a uintptr_t boundary.
	AfterPeb = (char*)(((uintptr_t)AfterPeb + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1));
	
	// Copy the command line.
	PPeb->CommandLine = AfterPeb;
	PPeb->CommandLineSize = strlen(CommandLine);
	strcpy(AfterPeb, CommandLine);
	
	AfterPeb += PPeb->CommandLineSize + 1;
	
	// Align to a uintptr_t boundary.
	AfterPeb = (char*)(((uintptr_t)AfterPeb + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1));
	
	// Copy the environment.
	PPeb->Environment = AfterPeb;
	PPeb->EnvironmentSize = PspEnvironmentLength(Environment);
	memcpy(PPeb->Environment, Environment, PPeb->EnvironmentSize);
	
	// Assign the PEB to this process, detach, and dereference the process object.
	Process->Pcb.PebPointer = PPeb;
	
	PsSetAttachedProcess(OldAttached);
	ObDereferenceObject(Process);
	
	void* EntryPoint = (void*) (BoronDllBase + ElfHeader.EntryPoint);
	
	PTHREAD_START_CONTEXT Context = MmAllocatePool(POOL_NONPAGED, sizeof(THREAD_START_CONTEXT));
	if (!Context)
		KeCrash("%s: Failed to create thread in process: Insufficient Memory", Func);
	
	Context->InstructionPointer = EntryPoint;
	Context->UserContext = PPeb;
	
	// OK. Now, the process is mapped.
	// Create a thread to make it call the entry point.
	HANDLE ThreadHandle;
	Status = PsCreateSystemThread(
		&ThreadHandle,
		NULL,
		ProcessHandle,
		PspUserThreadStart,
		Context,
		false
	);
	
	if (FAILED(Status))
		KeCrash("%s: Failed to create thread in process: %d (%s)", Func, Status, RtlGetStatusString(Status));
	
	OSClose(ThreadHandle);
	
	Status = OSWaitForSingleObject(ProcessHandle, false, TIMEOUT_INFINITE);
	
	// Maybe the init process isn't supposed to exit. I should think of that.
	if (FAILED(Status))
		KeCrash("%s: Init process has failed. Status: %d (%s)", Func, Status, RtlGetStatusString(Status));
	else
		LogMsg(ANSI_YELLOW "Info" ANSI_DEFAULT ": Init process has exited successfully.");
	
	OSClose(ProcessHandle);
	PsTerminateThread();
}

INIT
bool PsInitSystemPart2()
{
	PETHREAD Thread = NULL;
	BSTATUS Status;
	
	if (!PsShouldStartInitialProcess())
	{
		LogMsg("/NOINIT specified, not starting initial process.");
		return true;
	}
	
	Status = PsCreateSystemThreadFast(
		&Thread,
		PsStartInitialProcess,
		NULL,
		false
	);
	
	if (FAILED(Status))
		DbgPrint("Failed to setup initial process: %d", Status);
	
	if (Thread)
		ObDereferenceObject(Thread);
	
	return SUCCEEDED(Status);
}
