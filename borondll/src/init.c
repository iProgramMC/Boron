#include <boron.h>
#include <elf.h>
#include <rtl/assert.h>


HIDDEN
BSTATUS OSDLLMapElfFile(HANDLE Handle)
{
	BSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	ELF_HEADER ElfHeader;
	ELF_PROGRAM_HEADER ElfProgramHeader;
	
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
			DbgPrint("OSDLL: Skipping segment because its size in memory is zero...");
			continue;
		}
		
		int Protection = 0;
		int CowFlag = 0;
		
		if (ElfProgramHeader.Flags & 1) Protection |= PAGE_EXECUTE;
		if (ElfProgramHeader.Flags & 4) Protection |= PAGE_READ;
		if (ElfProgramHeader.Flags & 2) CowFlag = MEM_COW;
		
		if (Protection == 0)
		{
			DbgPrint("OSDLL: Skipping segment because its protection is zero...");
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
			void* Address = (void*) (ElfProgramHeader.VirtualAddress & (PAGE_SIZE - 1));
			MapSize += ElfProgramHeader.VirtualAddress & (PAGE_SIZE - 1);
			MapSize = (MapSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
			
		#ifdef DEBUG
			void* OldAddress = Address;
		#endif
			
			Status = OSAllocateVirtualMemory(
				CURRENT_PROCESS_HANDLE,
				&Address,
				&MapSize,
				MEM_COMMIT | MEM_RESERVE | MEM_FIXED,
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
		
		// Initialized data.
		Status = OSMapViewOfObject(
			CURRENT_PROCESS_HANDLE,
			Handle,
			&Address,
			ElfProgramHeader.SizeInMemory,
			MEM_COMMIT | MEM_RESERVE | MEM_FIXED | CowFlag,
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
	}
	
	return STATUS_SUCCESS;
}


HIDDEN
BSTATUS OSDLLRunImage(PPEB Peb)
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
	
	Status = OSDLLMapElfFile(Handle);
	
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
	
	return Status;
}


HIDDEN
void DLLEntryPoint(PPEB Peb)
{
	BSTATUS Status;
	
	DbgPrint("-- LibBoron.so init!");
	DbgPrint("Peb: %p", Peb);
	DbgPrint("Image Name: '%s'", Peb->ImageName);
	DbgPrint("Command Line: '%s'", Peb->CommandLine);
	
	Status = OSDLLRunImage(Peb);
	DbgPrint("OSDLLRunImage exited with status %d (%s)", Status, RtlGetStatusString(Status));
	
	OSExitThread();
}
