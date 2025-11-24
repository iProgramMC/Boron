#include <boron.h>
#include <elf.h>
#include <string.h>
#include <rtl/assert.h>
#include <rtl/cmdline.h>
#include "dll.h"
#include "pebteb.h"

extern HIDDEN ELF_DYNAMIC_ITEM _DYNAMIC[];

// Limitations of this ELF loader that I don't plan to solve:
//
// - No programs with an image base of 0
// - The misalignment of the file offset and the virtual address must match
// - The entire strtab section must be loaded inside a LOAD PHDR.
//
// TODO: Add better error propagation.
//

static LIST_ENTRY OSDllLoadQueue;
static LIST_ENTRY OSDllsLoaded;

// Adds a NEEDED entry, which resolves to an offset in strtab which
// will be resolved later.
HIDDEN
bool OSDLLAddDllToLoad(const char* DllName)
{
	size_t Length = strlen(DllName);
	PDLL_LOAD_QUEUE_ITEM QueueItem = OSAllocate(sizeof(DLL_LOAD_QUEUE_ITEM) + Length + 1);
	
	if (!QueueItem)
		return false;
	
	strcpy(QueueItem->PathName, DllName);
	InsertTailList(&OSDllLoadQueue, &QueueItem->ListEntry);
	return true;
}

HIDDEN
const char* OSDLLOffsetPathAway(const char* Name)
{
	if (!Name)
		return "Unknown";
	
	intptr_t Length = (intptr_t) strlen(Name);
	while (--Length >= 0)
	{
		if (Name[Length] == OB_PATH_SEPARATOR)
			return &Name[Length + 1];
	}
	
	return Name;
}

// TODO: If we need to implement loading libraries at runtime,
// we should protect this with a critical section!
HIDDEN
BSTATUS OSDLLMapElfFile(
	PPEB Peb,
	HANDLE ProcessHandle,
	HANDLE Handle,
	const char* Name,
	OSDLL_ENTRY_POINT* OutEntryPoint,
	int FileKind
)
{
	BSTATUS Status = STATUS_SUCCESS;
	IO_STATUS_BLOCK Iosb;
	ELF_HEADER ElfHeader;
	ELF_PROGRAM_HEADER ElfProgramHeader;
	PELF_DYNAMIC_ITEM DynamicTable = NULL;
	PLOADED_IMAGE LoadedImage;
	uint8_t* ProgramHeaders = NULL;
	uintptr_t ImageBase = 0;
	bool NeedFreeProgramHeaders = false;
	bool NeedReadAndMapFile = true;
	bool IsSeparateProcess = ProcessHandle != CURRENT_PROCESS_HANDLE;
	bool IsMainExecutable = FileKind == FILE_KIND_MAIN_EXECUTABLE;
	
	Name = OSDLLOffsetPathAway(Name);
	LdrDbgPrint("OSDLL: Mapping ELF file %s.", Name);
	
	if (IsMainExecutable)
	{
		// If this is the main executable, then check if it was mapped.
		//
		// If it was mapped, then we're all good to proceed without
		// loading it again.
		if (Peb->Loader.MappedImage)
		{
			LdrDbgPrint("OSDLL: Don't need to read and map %s, because it's already mapped.", Name);
			
			NeedReadAndMapFile = false;
			ImageBase = Peb->Loader.ImageBase;
			ProgramHeaders = Peb->Loader.ProgramHeaders;
			ElfHeader = *(ELF_HEADER*)(Peb->Loader.FileHeader);
		}
		else
		{
			LdrDbgPrint("OSDLL: %s is the main executable, but we need to read and map it.", Name);
		}
	}
	else
	{
		LdrDbgPrint("OSDLL: %s is a dynamic library, so we need to read and map it.", Name);
	}
	
	if (NeedReadAndMapFile)
	{
		Status = OSReadFile(&Iosb, Handle, 0, &ElfHeader, sizeof ElfHeader, 0);
		if (FAILED(Status))
		{
			DbgPrint(
				"OSDLL: Failed to read ELF header: %d (%s)",
				Status,
				RtlGetStatusString(Status)
			);
			return Status;
		}
		
		Status = RtlCheckValidity(&ElfHeader);
		if (FAILED(Status))
		{
			DbgPrint("OSDLL: The ELF file is invalid. %s (%d)", RtlGetStatusString(Status), Status);
			return Status;
		}
		
		ProgramHeaders = OSAllocate(ElfHeader.ProgramHeaderSize * ElfHeader.ProgramHeaderCount);
		if (!ProgramHeaders)
		{
			DbgPrint("OSDLL: Failed to read ELF program headers, because we ran out of memory!");
			return STATUS_INSUFFICIENT_MEMORY;
		}
		
		NeedFreeProgramHeaders = true;
		
		Status = OSReadFile(
			&Iosb,
			Handle,
			ElfHeader.ProgramHeadersOffset,
			ProgramHeaders,
			ElfHeader.ProgramHeaderCount * ElfHeader.ProgramHeaderSize,
			0
		);
		if (FAILED(Status))
		{
			DbgPrint(
				"OSDLL: Failed to read ELF program headers: %s (%d)",
				RtlGetStatusString(Status),
				Status
			);
			return Status;
		}
	}
	
	if (ElfHeader.Type != ELF_TYPE_EXECUTABLE && ElfHeader.Type != ELF_TYPE_DYNAMIC)
	{
		LdrDbgPrint("OSDLL: Cannot load anything other than ET_EXEC and ET_DYN at the moment.");
		return STATUS_UNIMPLEMENTED;
	}
	
	if (ElfHeader.Type == ELF_TYPE_DYNAMIC)
	{
		// Read the program headers and decide on the maximum address.
		uintptr_t MaximumAddress = 0;
		
		for (int i = 0; i < ElfHeader.ProgramHeaderCount; i++)
		{
			PELF_PROGRAM_HEADER Header = (void*)(ProgramHeaders + i * ElfHeader.ProgramHeaderSize);
			
			if (Header->Type != PROG_LOAD && Header->Type != PROG_DYNAMIC)
				continue;
			
			uintptr_t AddressEnd = (Header->VirtualAddress + Header->SizeInMemory + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
			
			if (MaximumAddress < AddressEnd)
				MaximumAddress = AddressEnd;
		}
		
		if (MaximumAddress == 0)
		{
			LdrDbgPrint("OSDLL: Cannot load %s because it has no program headers or data.", Name);
			Status = STATUS_INVALID_EXECUTABLE;
			goto EarlyExit;
		}
		
		void* BaseAddress = NULL;
		size_t RegionSize = (size_t) MaximumAddress;
		Status = OSAllocateVirtualMemory(
			ProcessHandle,
			&BaseAddress,
			&RegionSize,
			MEM_RESERVE | MEM_TOP_DOWN,
			0
		);
		
		if (FAILED(Status))
		{
			DbgPrint(
				"OSDLL: Cannot allocate space for %s: %s (%d)",
				Name,
				RtlGetStatusString(Status),
				Status
			);
			goto EarlyExit;
		}
		
		// Found it.
		LdrDbgPrint("OSDLL: %s's image base is %p (size is %zu)", Name, BaseAddress, RegionSize);
		ImageBase = (uintptr_t) BaseAddress;
		
		// TODO: Now, free the reserved memory.  But we may need to avoid a race condition
		// where someone else wants to reserve memory there.
		//
		// TODO: If you get STATUS_CONFLICTING_ADDRESSES, unmap everything and allow
		// ourselves to try again.
		Status = OSFreeVirtualMemory(
			ProcessHandle,
			BaseAddress,
			RegionSize,
			MEM_RELEASE
		);
		
		if (FAILED(Status))
		{
			DbgPrint(
				"OSDLL: While loading %s, something that shouldn't fail failed: %s (%d)",
				Name,
				RtlGetStatusString(Status),
				Status
			);
			goto EarlyExit;
		}
	}
	
	if (IsMainExecutable)
	{
		Peb->Loader.MappedImage = true;
		Peb->Loader.ImageBase = ImageBase;
	}
	
	// Step 1.  Parse program headers.
	for (int i = 0; i < ElfHeader.ProgramHeaderCount; i++)
	{
		ElfProgramHeader = *((ELF_PROGRAM_HEADER*)(ProgramHeaders + i * ElfHeader.ProgramHeaderSize));
		
		if (ElfProgramHeader.SizeInMemory == 0)
		{
			LdrDbgPrint("OSDLL: Skipping segment %d because its size in memory is zero...", i);
			continue;
		}
		
		switch (ElfProgramHeader.Type)
		{
			// a LOAD PHDR means that that segment must be mapped in from the source.
			case PROG_LOAD:
			{
				if (IsMainExecutable && !NeedReadAndMapFile)
				{
					LdrDbgPrint("OSDLL: Skipping load segment %d because main executable is already loaded.", i);
					break;
				}
				
				int Protection = 0;
				int CowFlag = 0;
				
				if (ElfProgramHeader.Flags & 1) Protection |= PAGE_EXECUTE;
				if (ElfProgramHeader.Flags & 4) Protection |= PAGE_READ;
				if (ElfProgramHeader.Flags & 2) CowFlag = MEM_COW;
				
				if (Protection == 0)
				{
					LdrDbgPrint("OSDLL: Skipping segment %d because its protection is zero...", i);
					continue;
				}
				
				// Check if the virtual address and offset within the file have the
				// same alignment.
				if ((ElfProgramHeader.Offset & (PAGE_SIZE - 1)) !=
					(ElfProgramHeader.VirtualAddress & (PAGE_SIZE - 1)))
				{
					LdrDbgPrint(
						"OSDLL: Don't support this program header where the offset within "
						"the file does not have the same alignment as its virtual address!"
					);
					Status = STATUS_UNIMPLEMENTED;
					goto EarlyExit;
				}
				
				if (ElfProgramHeader.SizeInFile == 0)
				{
					LdrDbgPrint("OSDLL: Uninitialized data at %p", ElfProgramHeader.VirtualAddress);
					
					// Make sure the represented range is aligned to page boundaries and covers
					// the entire range.
					size_t MapSize = ElfProgramHeader.SizeInMemory;
					void* Address = (void*) (ImageBase + (ElfProgramHeader.VirtualAddress & ~(PAGE_SIZE - 1)));
					MapSize += ElfProgramHeader.VirtualAddress & (PAGE_SIZE - 1);
					MapSize = (MapSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
					
				#ifdef DEBUG
					void* OldAddress = Address;
				#endif
					
					Status = OSAllocateVirtualMemory(
						ProcessHandle,
						&Address,
						&MapSize,
						MEM_FIXED | MEM_COMMIT | MEM_RESERVE,
						Protection
					);
					
					ASSERT(OldAddress == Address);
					
					if (FAILED(Status))
					{
						DbgPrint(
							"OSDLL: Failed to map uninitialized data for the program: %s (%d)",
							RtlGetStatusString(Status),
							Status
						);
						goto EarlyExit;
					}
					
					continue;
				}
				
				void* Address = (void*)(ImageBase + ElfProgramHeader.VirtualAddress);
				
			#ifdef DEBUG
				void* OldAddress = Address;
			#endif
				
				LdrDbgPrint("OSDLL: Initialized data at offset %p, size %zu.", OldAddress, ElfProgramHeader.SizeInMemory);
				
				// Initialized data.
				Status = OSMapViewOfObject(
					ProcessHandle,
					Handle,
					&Address,
					ElfProgramHeader.SizeInMemory,
					MEM_COMMIT | MEM_FIXED | CowFlag,
					ElfProgramHeader.Offset,
					Protection
				);
				
				ASSERT(OldAddress == Address);
					
				if (FAILED(Status))
				{
					DbgPrint(
						"OSDLL: Failed to map initialized data for the program: %p %zu %s (%d)",
						OldAddress,
						ElfProgramHeader.SizeInMemory,
						RtlGetStatusString(Status),
						Status
					);
					goto EarlyExit;
				}
				
				if (IsMainExecutable && ElfProgramHeader.Offset == 0)
					Peb->Loader.FileHeader = Address;
				
				if (ElfProgramHeader.SizeInFile < ElfProgramHeader.SizeInMemory)
				{
					// TODO: add a system call for this instead!
					size_t Offset = ElfProgramHeader.SizeInFile;
					size_t Size = ElfProgramHeader.SizeInMemory - ElfProgramHeader.SizeInFile;
					
					uint8_t* Data = OSAllocate(Size);
					if (!Data)
					{
						DbgPrint("OSDLL: Failed to map part of uninitialized data for the program due to insufficient memory");
						Status = STATUS_INSUFFICIENT_MEMORY;
						goto EarlyExit;
					}
					
					memset(Data, 0, Size);
					
					Status = OSWriteVirtualMemory(ProcessHandle, (char*) Address + Offset, Data, Size);
					OSFree(Data);
					
					if (FAILED(Status))
					{
						DbgPrint(
							"OSDLL: Failed to map part of uninitialized data for the program: %s (%d)",
							RtlGetStatusString(Status),
							Status
						);
						goto EarlyExit;
					}
				}
				
				break;
			}
			case PROG_DYNAMIC:
			{
				// Found the dynamic program header.
				DynamicTable = (PELF_DYNAMIC_ITEM)(ImageBase + ElfProgramHeader.VirtualAddress);
				break;
			}
			case PROG_PHDR:
			{
				if (IsMainExecutable)
					Peb->Loader.ProgramHeaders = (void*)(ImageBase + ElfProgramHeader.VirtualAddress);
				else
					LdrDbgPrint("OSDLL: Found PROG_PHDR in dynamic library.  It will be ignored.");
				
				break;
			}
			case PROG_INTERP:
			{
				if (IsMainExecutable)
					Peb->Loader.Interpreter = (const char*)(ImageBase + ElfProgramHeader.VirtualAddress);
				else
					LdrDbgPrint("OSDLL: Found PROG_INTERP in dynamic library.  It will be ignored.");
				
				break;
			}
			default:
			{
				LdrDbgPrint(
					"OSDLL: Skipping phdr %d because its type is %d.",
					i,
					ElfProgramHeader.Type
				);
			}
		}
	}
	
	// If this is a separate process, DO NOT attempt to relocate it.
	// The executable's own interpreter will do so.
	if (IsSeparateProcess)
	{
		Status = STATUS_SUCCESS;
		goto EarlyExit;
	}
	
	LoadedImage = OSAllocate(sizeof(LOADED_IMAGE));
	LoadedImage->FileKind = FileKind;
	StringCopySafe(LoadedImage->Name, Name, sizeof(LoadedImage->Name));
	
	// Parse the dynamic segment, if it exists.
	// If it doesn't exist, it's probably a statically linked program.
	if (DynamicTable &&
		!RtlParseDynamicTable(DynamicTable, &LoadedImage->DynamicInfo, ImageBase))
	{
		OSFree(LoadedImage);
		return STATUS_INVALID_EXECUTABLE;
	}
	
	LoadedImage->DynamicTable = DynamicTable;
	LoadedImage->ImageBase = ImageBase;
	InsertTailList(&OSDllsLoaded, &LoadedImage->ListEntry);
	
EarlyExit:
	if (ProgramHeaders && NeedFreeProgramHeaders)
		OSFree(ProgramHeaders);
	
	if (SUCCEEDED(Status) && OutEntryPoint)
		*OutEntryPoint = (OSDLL_ENTRY_POINT)(ImageBase + (uintptr_t)ElfHeader.EntryPoint);
	
	return Status;
}

HIDDEN
BSTATUS OSDLLLoadDynamicLibrary(PPEB Peb, const char* FileName)
{
	BSTATUS Status;
	HANDLE Handle;
	
	Status = OSDLLOpenFileByName(&Handle, FileName, true);
	if (FAILED(Status))
	{
		DbgPrint(
			"OSDLL: Failed to open %s: %d (%s)",
			FileName,
			Status,
			RtlGetStatusString(Status)
		);
		return Status;
	}
	
	OSDLL_ENTRY_POINT EntryPoint;
	Status = OSDLLMapElfFile(Peb, CURRENT_PROCESS_HANDLE, Handle, FileName, &EntryPoint, FILE_KIND_DYNAMIC_LIBRARY);
	OSClose(Handle);
	if (FAILED(Status))
		return Status;
	
	return STATUS_SUCCESS;
}

HIDDEN
void OSDLLAddSelfToDllList()
{
	PLOADED_IMAGE LoadedImage = OSAllocate(sizeof(LOADED_IMAGE));
	strcpy(LoadedImage->Name, "libboron.so");
	
	LoadedImage->ImageBase = RtlGetImageBase();
	LoadedImage->DynamicTable = _DYNAMIC;
	LoadedImage->FileKind = FILE_KIND_INTERPRETER;
	
#ifdef DEBUG
	// Assert that DYN_NEEDED is not specified.
	PELF_DYNAMIC_ITEM Item = _DYNAMIC;
	while (Item->Tag != DYN_NULL)
	{
		ASSERT(Item->Tag != DYN_NEEDED);
		Item++;
	}
#endif
	
	RtlParseDynamicTable(LoadedImage->DynamicTable, &LoadedImage->DynamicInfo, LoadedImage->ImageBase);
	
	InsertTailList(&OSDllsLoaded, &LoadedImage->ListEntry);
}

HIDDEN
BSTATUS OSDLLRunImage(PPEB Peb, OSDLL_ENTRY_POINT* OutEntryPoint)
{
	BSTATUS Status;
	
	Status = OSDLLCreateTeb(Peb);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Failed to create TEB! %s (%d)", RtlGetStatusString(Status), Status);
		return Status;
	}
	
	HANDLE FileHandle = HANDLE_NONE;
	
	if (Peb->Loader.MappedImage)
	{
		// Executable is mapped already, so instead of looking it up via the path,
		// obtain a handle to it via a known address.
		Status = OSGetMappedFileHandle(&FileHandle, CURRENT_PROCESS_HANDLE, (uintptr_t) Peb->Loader.FileHeader);
	}
	else
	{
		OBJECT_ATTRIBUTES Attributes;
		
		Attributes.ObjectName = Peb->ImageName;
		Attributes.ObjectNameLength = Peb->ImageNameSize;
		Attributes.OpenFlags = 0;
		Attributes.RootDirectory = HANDLE_NONE;
		
		Status = OSOpenFile(&FileHandle, &Attributes);
	}
	
	if (FAILED(Status))
	{
		DbgPrint(
			"OSDLL: Failed to open %s: %d (%s)",
			Peb->ImageName,
			Status,
			RtlGetStatusString(Status)
		);
		return Status;
	}
	
	Status = OSDLLMapElfFile(Peb, CURRENT_PROCESS_HANDLE, FileHandle, Peb->ImageName, OutEntryPoint, FILE_KIND_MAIN_EXECUTABLE);
	OSClose(FileHandle);
	
	if (FAILED(Status))
	{
		DbgPrint(
			"OSDLL: Failed to map ELF image %s: %d (%s)",
			Peb->ImageName,
			Status,
			RtlGetStatusString(Status)
		);
		return Status;
	}
	
	// Load the DLLs in the queue.
	while (!IsListEmpty(&OSDllLoadQueue))
	{
		PLIST_ENTRY Entry = OSDllLoadQueue.Flink;
		RemoveEntryList(Entry);
		
		PDLL_LOAD_QUEUE_ITEM QueueItem = CONTAINING_RECORD(Entry, DLL_LOAD_QUEUE_ITEM, ListEntry);
		
		bool NeedToLoad = true;
		
		// TODO: More efficient way to do this.
		// Check if the DLL is already loaded.
		PLIST_ENTRY DllEntry = OSDllsLoaded.Flink;
		while (DllEntry != &OSDllsLoaded)
		{
			PLOADED_IMAGE LoadedImage = CONTAINING_RECORD(DllEntry, LOADED_IMAGE, ListEntry);
			
			if (strcmp(LoadedImage->Name, QueueItem->PathName) == 0)
			{
				NeedToLoad = false;
				break;
			}
			
			DllEntry = DllEntry->Flink;
		}
		
		if (!NeedToLoad)
		{
			OSFree(QueueItem);
			continue;
		}
		
		LdrDbgPrint("OSDLL: Loading library %s.", QueueItem->PathName);
		Status = OSDLLLoadDynamicLibrary(Peb, QueueItem->PathName);
		
		if (FAILED(Status))
			DbgPrint("OSDLL: Cannot load library %s: %s (%d)", QueueItem->PathName, RtlGetStatusString(Status), Status);
		
		OSFree(QueueItem);
		
		if (FAILED(Status))
			return Status;
	}
	
	// Start linking the executables to each other.
	PLIST_ENTRY DllEntry = OSDllsLoaded.Flink;
	while (DllEntry != &OSDllsLoaded)
	{
		PLOADED_IMAGE LoadedImage = CONTAINING_RECORD(DllEntry, LOADED_IMAGE, ListEntry);
		
		// The interpreter knows how to relocate themselves and has already done so.
		if (LoadedImage->FileKind != FILE_KIND_INTERPRETER)
		{
			LdrDbgPrint("OSDLL: Linking %s, file kind: %d...", LoadedImage->Name, LoadedImage->FileKind);
			if (!RtlPerformRelocations(&LoadedImage->DynamicInfo, LoadedImage->ImageBase))
			{
				DbgPrint("OSDLL: Cannot perform relocations on module %s.", LoadedImage->Name);
				return STATUS_INVALID_EXECUTABLE;
			}
			
			RtlRelocateRelrEntries(&LoadedImage->DynamicInfo, LoadedImage->ImageBase);
			
			if (!RtlLinkPlt(&LoadedImage->DynamicInfo, LoadedImage->ImageBase, LoadedImage->Name))
			{
				DbgPrint("OSDLL: Module %s could not be linked.", LoadedImage->Name);
				return STATUS_INVALID_EXECUTABLE;
			}
		}
		
		// TODO: Change permissions so that the code segments cannot be written to anymore.
		
		DllEntry = DllEntry->Flink;
	}
	
	return Status;
}

HIDDEN
uintptr_t OSDLLGetProcedureAddressInModule(PLOADED_IMAGE Image, const char* ProcName)
{
	PELF_HASH_TABLE HashTable = Image->DynamicInfo.HashTable;
	PELF_SYMBOL DynSymTable = Image->DynamicInfo.DynSymTable;
	const char* DynStrTable = Image->DynamicInfo.DynStrTable;
	uint32_t* Chains = &HashTable->Data[HashTable->BucketCount];
	
	if (!HashTable || HashTable->BucketCount == 0)
	{
		LdrDbgPrint("OSDLL: Module %s does not have a hash table.", Image->Name);
		return 0;
	}
	
	uint32_t NameHash = RtlElfHash(ProcName);
	uint32_t BucketIndex = NameHash % HashTable->BucketCount;
	
	for (uint32_t SymbolIndex = HashTable->Data[BucketIndex];
	     SymbolIndex != 0; // STN_UNDEF
	     SymbolIndex = Chains[SymbolIndex])
	{
		PELF_SYMBOL Symbol = &DynSymTable[SymbolIndex];
		const char* Name = DynStrTable + Symbol->Name;
		
		if (strcmp(Name, ProcName) == 0)
			return Image->ImageBase + Symbol->Value;
	}
	
	return 0;
}

HIDDEN
uintptr_t OSDLLGetProcedureAddress(const char* ProcName)
{
	PLIST_ENTRY DllEntry = OSDllsLoaded.Flink;
	while (DllEntry != &OSDllsLoaded)
	{
		PLOADED_IMAGE LoadedImage = CONTAINING_RECORD(DllEntry, LOADED_IMAGE, ListEntry);
		
		uintptr_t Address = OSDLLGetProcedureAddressInModule(LoadedImage, ProcName);
		if (Address)
		{
			LdrDbgPrint(
				"OSDLL: Found procedure %s in module %s at address %p.",
				ProcName,
				LoadedImage->Name,
				Address
			);
			
			return Address;
		}
		
		DllEntry = DllEntry->Flink;
	}
	
	LdrDbgPrint("OSDLL: Cannot find address of procedure %s.", ProcName);
	return 0;
}

BSTATUS OSDLLSetupArgumentsAndEnvironment(PPEB Peb, int* OutArgumentCount, char*** OutArgumentArray, char*** OutEnvironmentArray)
{
	// Note: Argument count is biased by one because the image name is an argument.
	int ArgumentCount = 1 + (Peb->CommandLine ? RtlEnvironmentCount(Peb->CommandLine) : 0);
	int EnvironmentCount =   Peb->Environment ? RtlEnvironmentCount(Peb->Environment) : 0;
	
	char** ArgumentArray    = OSAllocate(sizeof(char*) * (ArgumentCount + 1));
	char** EnvironmentArray = OSAllocate(sizeof(char*) * (EnvironmentCount + 1));
	if (!ArgumentArray || !EnvironmentArray)
	{
		DbgPrint("OSDLL: Out of memory while allocating ArgumentArray or EnvironmentArray.");
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	// The first argument is the image's name.  The last item in
	// the argument array is NULL.
	ArgumentArray[0] = Peb->ImageName ? Peb->ImageName : "unknown";
	ArgumentArray[ArgumentCount] = NULL;
	
	// The environment array is also terminated with NULL.
	EnvironmentArray[EnvironmentCount] = NULL;
	
	char* ArgumentPointer = Peb->CommandLine;
	for (int i = 1; i < ArgumentCount; i++)
	{
		ArgumentArray[i] = ArgumentPointer;
		ArgumentPointer += strlen(ArgumentPointer) + 1;
	}
	
	char* EnvironmentPointer = Peb->Environment;
	for (int i = 0; i < EnvironmentCount; i++)
	{
		EnvironmentArray[i] = EnvironmentPointer;
		EnvironmentPointer += strlen(EnvironmentPointer) + 1;
	}
	
	*OutArgumentCount = ArgumentCount;
	*OutArgumentArray = ArgumentArray;
	*OutEnvironmentArray = EnvironmentArray;
	return STATUS_SUCCESS;
}

NO_RETURN HIDDEN
void DLLEntryPoint(PPEB Peb)
{
	OSDLLInitializeGlobalHeap();
	InitializeListHead(&OSDllLoadQueue);
	InitializeListHead(&OSDllsLoaded);
	OSDLLAddSelfToDllList();
	
	BSTATUS Status;
	
	LdrDbgPrint("OSDLL: DLLEntryPoint!");
	LdrDbgPrint("OSDLL: Peb:          %p",   Peb);
	LdrDbgPrint("OSDLL: Free Size:    %zu",  Peb->PebFreeSize);
	LdrDbgPrint("OSDLL: Image Name:   '%s'", Peb->ImageName);
	LdrDbgPrint("OSDLL: Command Line: '%s'", Peb->CommandLine ? Peb->CommandLine : "(none)");
	LdrDbgPrint("OSDLL: Environment:  '%s'", Peb->Environment ? Peb->Environment : "(none)");
	
	OSDLL_ENTRY_POINT EntryPoint = NULL;
	Status = OSDLLRunImage(Peb, &EntryPoint);
	LdrDbgPrint("OSDLL: OSDLLRunImage exited with status %d (%s)", Status, RtlGetStatusString(Status));
	
	if (FAILED(Status))
		OSExitProcess(Status);
	
	int ArgumentCount;
	char** ArgumentArray, **EnvironmentArray;
	Status = OSDLLSetupArgumentsAndEnvironment(Peb, &ArgumentCount, &ArgumentArray, &EnvironmentArray);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Failed to set up arguments and environment: %s (%d)", RtlGetStatusString(Status), Status);
		OSExitProcess(Status);
	}
	
	LdrDbgPrint("OSDLL: Calling entry point %p...", EntryPoint);
	Status = EntryPoint(ArgumentCount, ArgumentArray, EnvironmentArray);
	
	LdrDbgPrint("OSDLL: %s exited with code %d.", Peb->ImageName, Status);
	OSExitProcess(Status);
}
