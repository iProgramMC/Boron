#include <boron.h>
#include <elf.h>
#include <string.h>
#include <rtl/assert.h>
#include "dll.h"
#include "pebteb.h"

typedef int(*ELF_ENTRY_POINT2)();

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
BSTATUS OSDLLMapElfFile(HANDLE Handle, const char* Name, ELF_ENTRY_POINT2* OutEntryPoint)
{
	BSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	ELF_HEADER ElfHeader;
	ELF_PROGRAM_HEADER ElfProgramHeader;
	PELF_DYNAMIC_ITEM DynamicTable = NULL;
	PLOADED_IMAGE LoadedImage;
	
	Name = OSDLLOffsetPathAway(Name);
	DbgPrint("OSDLL: Mapping ELF file %s.", Name);
	
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
	
	uintptr_t ImageBase = 0;
	
	// TODO: Decide on an image base automatically for shared objects.
	if (ElfHeader.Type != ELF_TYPE_EXECUTABLE)
	{
		DbgPrint("OSDLL: Cannot load anything other than ET_EXEC at the moment.");
		return STATUS_UNIMPLEMENTED;
	}
	
	// Step 1.  Parse program headers.
	for (int i = 0; i < ElfHeader.ProgramHeaderCount; i++)
	{
		Status = OSReadFile(
			&Iosb,
			Handle,
			ElfHeader.ProgramHeadersOffset + ElfHeader.ProgramHeaderSize * i,
			&ElfProgramHeader,
			sizeof ElfProgramHeader,
			0
		);
		
		if (FAILED(Status))
		{
			DbgPrint(
				"OSDLL: Failed to read ELF program header: %d (%s)",
				Status,
				RtlGetStatusString(Status)
			);
			return Status;
		}
		
		if (ElfProgramHeader.SizeInMemory == 0)
		{
			DbgPrint("OSDLL: Skipping segment %d because its size in memory is zero...", i);
			continue;
		}
		
		switch (ElfProgramHeader.Type)
		{
			// a LOAD PHDR means that that segment must be mapped in from the source.
			case PROG_LOAD:
			{
				int Protection = 0;
				int CowFlag = 0;
				
				if (ElfProgramHeader.Flags & 1) Protection |= PAGE_EXECUTE;
				if (ElfProgramHeader.Flags & 4) Protection |= PAGE_READ;
				if (ElfProgramHeader.Flags & 2) CowFlag = MEM_COW;
				
				if (Protection == 0)
				{
					DbgPrint("OSDLL: Skipping segment %d because its protection is zero...", i);
					continue;
				}
				
				// Check if the virtual address and offset within the file have the
				// same alignment.
				if ((ElfProgramHeader.Offset & (PAGE_SIZE - 1)) !=
					(ElfProgramHeader.VirtualAddress & (PAGE_SIZE - 1)))
				{
					DbgPrint(
						"OSDLL: Don't support this program header where the offset within "
						"the file does not have the same alignment as its virtual address!"
					);
					return STATUS_UNIMPLEMENTED;
				}
				
				if (ElfProgramHeader.SizeInFile == 0)
				{
					DbgPrint("OSDLL: Uninitialized data at %p", ElfProgramHeader.VirtualAddress);
					
					// Make sure the represented range is aligned to page boundaries and covers
					// the entire range.
					size_t MapSize = ElfProgramHeader.SizeInMemory;
					void* Address = (void*) (ElfProgramHeader.VirtualAddress & ~(PAGE_SIZE - 1));
					MapSize += ElfProgramHeader.VirtualAddress & (PAGE_SIZE - 1);
					MapSize = (MapSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
					
				#ifdef DEBUG
					void* OldAddress = Address;
				#endif
					
					Status = OSAllocateVirtualMemory(
						CURRENT_PROCESS_HANDLE,
						&Address,
						&MapSize,
						MEM_COMMIT | MEM_RESERVE,
						Protection
					);
					
					ASSERT(OldAddress == Address);
					
					if (FAILED(Status))
					{
						DbgPrint(
							"OSDLL: Failed to map uninitialized data in ELF: %d (%s)",
							Status,
							RtlGetStatusString(Status)
						);
						return Status;
					}
					
					continue;
				}
				
				void* Address = (void*) ElfProgramHeader.VirtualAddress;
				
			#ifdef DEBUG
				void* OldAddress = Address;
			#endif
				
				DbgPrint("Initialized data at offset %p.", OldAddress);
				
				// Initialized data.
				Status = OSMapViewOfObject(
					CURRENT_PROCESS_HANDLE,
					Handle,
					&Address,
					ElfProgramHeader.SizeInMemory,
					MEM_COMMIT | CowFlag,
					ElfProgramHeader.Offset,
					Protection
				);
				
				ASSERT(OldAddress == Address);
					
				if (FAILED(Status))
				{
					DbgPrint(
						"OSDLL: Failed to map initialized data in ELF: %d (%s)",
						Status,
						RtlGetStatusString(Status)
					);
					return Status;
				}
				break;
			}
			case PROG_DYNAMIC:
			{
				// Found the dynamic program header.
				DynamicTable = (PELF_DYNAMIC_ITEM)(ImageBase + ElfProgramHeader.VirtualAddress);
				break;
			}
			default:
			{
				DbgPrint(
					"OSDLL: Skipping phdr %d because its type is %d.",
					i,
					ElfProgramHeader.Type
				);
			}
		}
	}
	
	LoadedImage = OSAllocate(sizeof(LOADED_IMAGE));
	StringCopySafe(LoadedImage->Name, Name, sizeof(LoadedImage->Name));
	
	// Step 2.  Parse the dynamic segment, if it exists.
	// If it doesn't exist, it's probably a statically linked program.
	if (DynamicTable &&
		!RtlParseDynamicTable(DynamicTable, &LoadedImage->DynamicInfo, ImageBase))
	{
		OSFree(LoadedImage);
		return STATUS_INVALID_EXECUTABLE;
	}
	
	LoadedImage->DynamicTable = DynamicTable;	
	InsertTailList(&OSDllsLoaded, &LoadedImage->ListEntry);
	
	*OutEntryPoint = (ELF_ENTRY_POINT2)(ImageBase + (uintptr_t)ElfHeader.EntryPoint);
	
	return STATUS_SUCCESS;
}

HIDDEN
void OSDLLAddSelfToDllList()
{
	PLOADED_IMAGE LoadedImage = OSAllocate(sizeof(LOADED_IMAGE));
	strcpy(LoadedImage->Name, "libboron.so");
	
	LoadedImage->ImageBase = RtlGetImageBase();
	LoadedImage->DynamicTable = _DYNAMIC;
	
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
BSTATUS OSDLLRunImage(PPEB Peb, ELF_ENTRY_POINT2* OutEntryPoint)
{
	BSTATUS Status;
	HANDLE Handle;
	OBJECT_ATTRIBUTES Attributes;
	
	Attributes.ObjectName = Peb->ImageName;
	Attributes.ObjectNameLength = Peb->ImageNameSize;
	Attributes.OpenFlags = 0;
	Attributes.RootDirectory = HANDLE_NONE;
	
	Status = OSOpenFile(&Handle, &Attributes);
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
	
	Status = OSDLLMapElfFile(Handle, Peb->ImageName, OutEntryPoint);
	OSClose(Handle);
	
	if (FAILED(Status))
	{
		DbgPrint(
			"OSDLL: Failed to map ELF image %s: %d (%s)",
			Peb->ImageName,
			Status,
			RtlGetStatusString(Status)
		);
	}
	
	// TODO: Load the DLLs in the queue.
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
				DbgPrint("OSDLL: Already loaded library %s.", QueueItem->PathName);
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
		
		// TODO: Load the library.
		DbgPrint("OSDLL: Need library %s.", QueueItem->PathName);
		OSFree(QueueItem);
	}
	
	// Start linking the executables to each other.
	PLIST_ENTRY DllEntry = OSDllsLoaded.Flink;
	while (DllEntry != &OSDllsLoaded)
	{
		PLOADED_IMAGE LoadedImage = CONTAINING_RECORD(DllEntry, LOADED_IMAGE, ListEntry);
		
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
		DbgPrint("OSDLL: Module %s does not have a hash table.", Image->Name);
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
	
	DbgPrint("OSDLL: Cannot find address of procedure %s in module %s.", ProcName, Image->Name);
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
			DbgPrint(
				"OSDLL: Found procedure %s in module %s at address %p.",
				ProcName,
				LoadedImage->Name,
				Address
			);
			
			return Address;
		}
		
		DllEntry = DllEntry->Flink;
	}
	
	DbgPrint("OSDLL: Cannot find address of procedure %s.", ProcName);
	return 0;
}

HIDDEN
void DLLEntryPoint(PPEB Peb)
{
	OSDLLInitializeGlobalHeap();
	InitializeListHead(&OSDllLoadQueue);
	InitializeListHead(&OSDllsLoaded);
	OSDLLAddSelfToDllList();
	
	BSTATUS Status;
	
	DbgPrint("-- LibBoron.so init!");
	DbgPrint("Peb: %p", Peb);
	DbgPrint("Image Name: '%s'", Peb->ImageName);
	DbgPrint("Command Line: '%s'", Peb->CommandLine);
	
	ELF_ENTRY_POINT2 EntryPoint = NULL;
	Status = OSDLLRunImage(Peb, &EntryPoint);
	DbgPrint("OSDLLRunImage exited with status %d (%s)", Status, RtlGetStatusString(Status));
	
	if (SUCCEEDED(Status))
	{
		Status = OSDLLCreateTeb(Peb);
		if (SUCCEEDED(Status))
		{
			DbgPrint("OSDLL: Calling entry point %p...", EntryPoint);
			Status = EntryPoint();
		}
		else
		{
			DbgPrint("OSDLL: Failed to create TEB! %d (%s)", Status, RtlGetStatusString(Status));
		}
	}
	
	DbgPrint("OSDLL: %s exited with code %d.", Peb->ImageName, Status);
	OSExitProcess(Status);
}
