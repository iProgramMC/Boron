#include <boron.h>
#include <string.h>
#include <rtl/elf.h>

#ifdef IS_64_BIT
#define USER_SPACE_END 0x0000800000000000
#else
#define USER_SPACE_END 0x80000000
#endif

static PELF_PROGRAM_HEADER OSDLLFindDynamicPhdr(
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

static BSTATUS OSDLLOpenSelf(PHANDLE FileHandle)
{
	extern char _DYNAMIC[];
	return OSGetMappedFileHandle(FileHandle, CURRENT_PROCESS_HANDLE, (uintptr_t) _DYNAMIC);
}

static BSTATUS OSDLLMapSelfIntoProcess(HANDLE ProcessHandle, HANDLE FileHandle, size_t PebSize, void** EntryPoint)
{
	const char* Func;
	BSTATUS Status;
	uint64_t FileSize = 0;
	ELF_HEADER ElfHeader;
	
	Func = __func__;
	
	Status = OSGetLengthFile(FileHandle, &FileSize);
	if (FAILED(Status))
	{
		DbgPrint("%s: Failed to get length of libboron: %d (%s)", Func, Status, RtlGetStatusString(Status));
		return Status;
	}
	
	// XXX: Arbitrary
	if (FileSize > 0x800000)
	{
		DbgPrint("%s: libboron is just too big", Func);
		return Status;
	}
	
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
	{
		DbgPrint("%s: Could not map libboron into memory: %d (%s)", Func, Status, RtlGetStatusString(Status));
		return Status;
	}
	
	DbgPrint("Mapped entire file to %p", ElfMappingBase);
	
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
	PELF_PROGRAM_HEADER DynamicPhdr = OSDLLFindDynamicPhdr(
		ProgramHeaders,
		ElfHeader.ProgramHeaderSize,
		ElfHeader.ProgramHeaderCount
	);
	
	if (!DynamicPhdr)
	{
		DbgPrint("%s: Error, cannot find dynamic program header in libboron.so", Func);
		return STATUS_INVALID_EXECUTABLE;
	}
	
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
	{
		DbgPrint("%s: Error, first address %p > largest address %p", Func, FirstAddr, LargestAddr);
		return STATUS_INVALID_EXECUTABLE;
	}
	
	uintptr_t Size = LargestAddr - FirstAddr;
	Size = (Size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	
	BoronDllBase = USER_SPACE_END - PebSize - Size - 0x1000;
	DbgPrint("BoronDllBase: %p", BoronDllBase);
	
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
			DbgPrint(
				"%s: Failed to map ELF phdr in: %d (%s)\n"
				"ProgramHeader->SizeInMemory: %zu\n"
				"ProgramHeader->VirtualAddress: %p\n"
				"ProgramHeader->Offset: %p", Func,
				Status,
				RtlGetStatusString(Status),
				ProgramHeader->SizeInMemory,
				ProgramHeader->VirtualAddress,
				ProgramHeader->Offset
			);
			return Status;
		}
	}
	
	// Program headers have been mapped.
	BoronDllBase = BoronDllBaseOld;
	
	// This assumes that the dynamic table is entirely mapped.
	if (!IsDynamicLoaded)
	{
		DbgPrint("%s: There is no LOAD segment that maps the DYNAMIC table.", Func);
		return STATUS_INVALID_EXECUTABLE;
	}
	
	// Make sure to unmap from the system address space.
	Status = OSFreeVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		ElfMappingBase,
		RegionSize,
		MEM_RELEASE
	);
	
	if (FAILED(Status))
	{
		DbgPrint("%s: Failed to free initial mapping of ELF: %d (%s)\n%p %zu", Func, Status, RtlGetStatusString(Status), ElfMappingBase, RegionSize);
		return Status;
	}
	
	ElfMappingBase = NULL;
	*EntryPoint = (void*) (BoronDllBase + ElfHeader.EntryPoint);
	
	return STATUS_SUCCESS;
}

static size_t OSDLLCalculatePebSize(const char* ImageName, const char* CommandLine)
{
	size_t PebSize = sizeof(PEB);
	PebSize += strlen(ImageName) + 8 + sizeof(uintptr_t);
	PebSize += strlen(CommandLine) + 8 + sizeof(uintptr_t);
	PebSize = (PebSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);	
	return PebSize;
}

static BSTATUS OSDLLPreparePebForProcess(
	HANDLE ProcessHandle,
	size_t PebSize,
	const char* ImageName,
	const char* CommandLine,
	void** PebPtrOut
)
{
	BSTATUS Status;
	
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
	
	// TODO: Copy the environment variables from our PEB into
	// the PEB of the receiving process
	
	// Allocate and copy the PEB into the executable's memory.
	void* PebPtr = NULL;
	size_t RegionSize = PebSize;
	Status = OSAllocateVirtualMemory(ProcessHandle, &PebPtr, &RegionSize, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_READ | PAGE_WRITE);
	if (FAILED(Status))
		goto Fail;
	
	DbgPrint("PEB Pointer: %p,  Size: %zu,  RgnSize: %zu", PebPtr, PebSize, RegionSize);
	Peb->PebFreeSize = RegionSize;
	
	// Now copy the PEB into the destination application.
	Status = OSWriteVirtualMemory(ProcessHandle, PebPtr, Peb, PebSize);
	if (FAILED(Status))
	{
		DbgPrint("Failed to OSWriteVirtualMemory: %d (%s)", Status, RtlGetStatusString(Status));
		goto Fail;
	}
	
	*PebPtrOut = PebPtr;
	
Fail:
	OSFree(Peb);
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
	void *EntryPoint = NULL, *PebPtr = NULL;
	
	BSTATUS Status = OSCreateProcessInternal(
		OutHandle,
		ObjectAttributes,
		CURRENT_PROCESS_HANDLE,
		InheritHandles
	);
	if (FAILED(Status))
		return Status;
	
	ProcessHandle = *OutHandle;
	
	size_t PebSize = OSDLLCalculatePebSize(ImageName, CommandLine);
	
	HANDLE FileHandle;
	Status = OSDLLOpenSelf(&FileHandle);
	if (FAILED(Status))
	{
	Fail:
		OSClose(ProcessHandle);
		return Status;
	}
	
	Status = OSDLLMapSelfIntoProcess(ProcessHandle, FileHandle, PebSize, &EntryPoint);
	OSClose(FileHandle);
	
	if (FAILED(Status))
		goto Fail;
	
	Status = OSDLLPreparePebForProcess(ProcessHandle, PebSize, ImageName, CommandLine, &PebPtr);
	if (FAILED(Status))
		goto Fail;
	
	Status = OSDLLCreateMainThread(ProcessHandle, OutMainThreadHandle, EntryPoint, PebPtr, CreateSuspended);
	if (FAILED(Status))
		goto Fail;
	
	return STATUS_SUCCESS;
}
