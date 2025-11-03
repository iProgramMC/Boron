/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	file.c
	
Abstract:
	This module implements the file object and FCB dispatcher
	for the temporary file system.
	
Author:
	iProgramInCpp - 3 November 2025
***/
#include "tmpfs.h"

#define TMPFS_TIMEOUT 2000

#define EXT(Fcb) ((PTMPFS_EXT) (Fcb)->Extension)

#define PREP_EXT PTMPFS_EXT Ext = EXT(Fcb)
#define PREP_EXT_PARENT PTMPFS_EXT ParentExt = EXT(ParentFcb)

// LOCK ORDER: Do *NOT* enter the parent mutex, and THEN a child's mutex.
// The lock ordering has been established to be backwards because of TmpRemoveDir.

static uint64_t NextTmpfsInodeNumber = 1;

void TmpReferenceFcb(PFCB Fcb)
{
	PREP_EXT;
	AtAddFetch(Ext->ReferenceCount, 1);
}

void TmpDereferenceFcb(PFCB Fcb)
{
	PREP_EXT;
	if (AtAddFetch(Ext->ReferenceCount, -1) == 0)
	{
		DbgPrint("TmpReferenceFcb(%p) DELETING!", Fcb);
		IoFreeFcb(Fcb);
	}
}

bool TmpFileSeekable(UNUSED PFCB Fcb)
{
	return true;
}

BSTATUS TmpResize(PFCB Fcb, uint64_t NewLength);

BSTATUS TmpBackingMemory(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset)
{
	PREP_EXT;
	if (Fcb->FileType == FILE_TYPE_DIRECTORY)
		return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
	
	if (Offset >= Fcb->FileLength)
		return IOSB_STATUS(Iosb, STATUS_HARDWARE_IO_ERROR);
	
	BSTATUS Status = KeWaitForSingleObject(&Ext->Lock, true, TMPFS_TIMEOUT, KeGetPreviousMode());
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	if (Ext->PageListCount == 0 && Fcb->FileLength != 0)
	{
		BSTATUS Status = TmpResize(Fcb, Fcb->FileLength);
		if (FAILED(Status))
		{
			KeReleaseMutex(&Ext->Lock);
			return IOSB_STATUS(Iosb, Status);
		}
	}
	
	Offset /= PAGE_SIZE;
	ASSERT(Offset < Ext->PageListCount);
	
	if (!Ext->PageList[Offset])
	{
		// Allocate a new page, fill it in with the initial memory if needed,
		// and assign it to the page list.
		MMPFN Pfn = MmAllocatePhysicalPage();
		if (Pfn == PFN_INVALID)
		{
			// uh oh.
			DbgPrint("TmpBackingMemory not enough memory!");
			KeReleaseMutex(&Ext->Lock);
			return IOSB_STATUS(Iosb, STATUS_INSUFFICIENT_MEMORY);
		}
		
		Ext->PageList[Offset] = Pfn;
		
		DbgPrint("InitialAddress: %p   Length: %zu  Offset: %zu", Ext->InitialAddress, Ext->InitialLength, Offset * PAGE_SIZE);
		
		// Physical pages are initialized to zero automatically.
		if (Ext->InitialAddress && Offset * PAGE_SIZE < Ext->InitialLength)
		{
			MmBeginUsingHHDM();
			void* AddressDest = MmGetHHDMOffsetAddrPfn(Pfn);
			
			size_t Size = PAGE_SIZE;
			if (Size > Ext->InitialLength - Offset * PAGE_SIZE)
				Size = Ext->InitialLength - Offset * PAGE_SIZE;
			
			memcpy(AddressDest, (uint8_t*) Ext->InitialAddress + Offset * PAGE_SIZE, Size);
			MmEndUsingHHDM();
		}
	}
	
	Iosb->BackingMemory.PhysicalAddress = Ext->PageList[Offset] * PAGE_SIZE;
	MmPageAddReference(Ext->PageList[Offset]);
	
	KeReleaseMutex(&Ext->Lock);
	return IOSB_STATUS(Iosb, STATUS_SUCCESS);
}

BSTATUS TmpRead(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, uint32_t Flags)
{
	// TODO
	(void) Fcb;
	(void) Offset;
	(void) MdlBuffer;
	(void) Flags;
	DbgPrint("NYI TmpRead.  Use the cache instead!");
	return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
}

BSTATUS TmpWrite(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, uint32_t Flags)
{
	// TODO
	(void) Fcb;
	(void) Offset;
	(void) MdlBuffer;
	(void) Flags;
	DbgPrint("NYI TmpWrite.  Use the cache instead!");
	return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
}

BSTATUS TmpResize(PFCB Fcb, uint64_t NewLength)
{
	PREP_EXT;
	uint64_t LengthPages = ((NewLength + PAGE_SIZE - 1) / PAGE_SIZE);
#ifdef IS_32_BIT
	if (LengthPages >= 524288)
		return STATUS_INSUFFICIENT_MEMORY;
#endif
	
	BSTATUS Status = KeWaitForSingleObject(&Ext->Lock, true, TMPFS_TIMEOUT, KeGetPreviousMode());
	if (FAILED(Status))
		return Status;
	
	uint64_t OldLength = Fcb->FileLength;
	Fcb->FileLength = NewLength;
	
	if (Ext->PageListCount < LengthPages)
	{
		size_t NewPageListCount = Ext->PageListCount;
		while (NewPageListCount < (size_t) LengthPages)
		{
			if (NewPageListCount < 2)
				NewPageListCount = 2;
			
			size_t NewNewPLC = NewPageListCount * 3 / 2;
			if (NewNewPLC < NewPageListCount)
			{
				// overflow
				NewPageListCount = (size_t) LengthPages;
				break;
			}
			
			NewPageListCount = NewNewPLC;
		}
		
		PMMPFN NewPageList = MmAllocatePool(POOL_NONPAGED, sizeof(MMPFN) * NewPageListCount);
		if (!NewPageList)
		{
			Fcb->FileLength = OldLength;
			KeReleaseMutex(&Ext->Lock);
			return STATUS_INSUFFICIENT_MEMORY;
		}
		
		if (Ext->PageList)
		{
			memcpy(NewPageList, Ext->PageList, sizeof(MMPFN) * Ext->PageListCount);
			memset(NewPageList + Ext->PageListCount, 0, sizeof(MMPFN) * (NewPageListCount - Ext->PageListCount));
			MmFreePool(Ext->PageList);
		}
		else
		{
			memset(NewPageList, 0, sizeof(MMPFN) * NewPageListCount);
		}
		
		Ext->PageList = NewPageList;
		Ext->PageListCount = NewPageListCount;
	}
	else if (LengthPages == 0)
	{
		// Free everything.
		for (size_t i = 0; i < Ext->PageListCount; i++)
			MmFreePhysicalPage(Ext->PageList[i]);
		
		MmFreePool(Ext->PageList);
		Ext->PageList = NULL;
		Ext->PageListCount = 0;
	}
	else if (Ext->PageListCount > LengthPages * 3)
	{
		// Try and shrink it.
		size_t NewPageListCount = (size_t) LengthPages;
		PMMPFN NewPageList = MmAllocatePool(POOL_NONPAGED, sizeof(MMPFN) * NewPageListCount);
		if (!NewPageList)
		{
			// Can't reduce the size of the page list, but this is a non-fatal error.
			// It'll just be bigger than necessary for a bit.
			KeReleaseMutex(&Ext->Lock);
			return STATUS_SUCCESS;
		}
		
		if (Ext->PageList)
		{
			memcpy(NewPageList, Ext->PageList, sizeof(MMPFN) * NewPageListCount);
			
			// Free all the cut-off pages.
			for (size_t i = NewPageListCount; i < Ext->PageListCount; i++)
				MmFreePhysicalPage(Ext->PageList[i]);
			
			MmFreePool(Ext->PageList);
		}
		else
		{
			memset(NewPageList, 0, sizeof(MMPFN) * NewPageListCount);
		}
		
		Ext->PageList = NewPageList;
		Ext->PageListCount = NewPageListCount;
	}
	
	KeReleaseMutex(&Ext->Lock);
	return STATUS_SUCCESS;
}

BSTATUS TmpMakeFile(PFCB ContainingFcb, PIO_DIRECTORY_ENTRY Entry)
{
	BSTATUS Status;
	PFILE_OBJECT FileObject;
	
	Status = TmpCreateFile(&FileObject, NULL, 0, FILE_TYPE_FILE, ContainingFcb, Entry->Name);
	if (FAILED(Status))
		return Status;
	
	ObDereferenceObject(FileObject);
	return Status;
}

BSTATUS TmpMakeDir(PFCB ContainingFcb, PIO_DIRECTORY_ENTRY Entry)
{
	BSTATUS Status;
	PFILE_OBJECT FileObject;
	
	Status = TmpCreateFile(&FileObject, NULL, 0, FILE_TYPE_DIRECTORY, ContainingFcb, Entry->Name);
	if (FAILED(Status))
		return Status;
	
	ObDereferenceObject(FileObject);
	return Status;
}

typedef union
{
	struct
	{
	#ifdef IS_64_BIT
		uint64_t Pointer : 48;
		uint64_t FileIndex : 16;
	#else
		uint64_t Pointer : 32;
		uint64_t FileIndex : 32;
	#endif
	}
	PACKED;
	
	uint64_t Offset;
}
TMPFS_OFFSET;

BSTATUS TmpReadDir(PIO_STATUS_BLOCK Iosb, PFILE_OBJECT FileObject, uint64_t Offset, uint64_t Version, PIO_DIRECTORY_ENTRY Entry)
{
	PFCB Fcb = FileObject->Fcb;
	PREP_EXT;
	
	if (Fcb->FileType != FILE_TYPE_DIRECTORY)
		return STATUS_NOT_A_DIRECTORY;
	
	BSTATUS Status = STATUS_SUCCESS;
	
	Status = KeWaitForSingleObject(&Ext->Lock, true, TMPFS_TIMEOUT, KeGetPreviousMode());
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	TMPFS_OFFSET TOffset;
	TOffset.Offset = Offset;
	
	if (Version != Ext->Version)
	{
		Version = Ext->Version;
		
		// Retrack the pointer.
		PLIST_ENTRY Entry = Ext->ChildrenListHead.Flink;
		for (int i = 0; i < TOffset.FileIndex; i++)
		{
			Entry = Entry->Flink;
			if (Entry == &Ext->ChildrenListHead)
			{
				// Reached end.
				KeReleaseMutex(&Ext->Lock);
				return IOSB_STATUS(Iosb, STATUS_END_OF_FILE);
			}
		}
		
		TOffset.Pointer = (uint64_t) Entry;
	}
	
	// Version is the same.
#ifdef IS_64_BIT
	if (TOffset.Pointer == 0xFFFFFFFFFFFF)
#else
	if (TOffset.Pointer == 0xFFFFFFFF)
#endif
	{
		KeReleaseMutex(&Ext->Lock);
		return IOSB_STATUS(Iosb, STATUS_END_OF_FILE);
	}
	
	PLIST_ENTRY DirListEntry;
#ifdef IS_64_BIT
	DirListEntry = (PLIST_ENTRY)(0xFFFF000000000000 | TOffset.Pointer);
#else
	DirListEntry = (PLIST_ENTRY) TOffset.Pointer;
#endif
	
	PTMPFS_DIR_ENTRY DirEntry = CONTAINING_RECORD(DirListEntry, TMPFS_DIR_ENTRY, ListEntry);
	ASSERT(DirEntry->ParentFcb == Fcb);
	
	// Prepare the next data.
	PLIST_ENTRY NextEntry = DirListEntry->Flink;
	if (NextEntry == &Ext->ChildrenListHead)
	{
	#ifdef IS_64_BIT
		TOffset.Pointer = 0xFFFFFFFFFFFF;
		TOffset.FileIndex = 0xFFFF;
	#else
		TOffset.Pointer = 0xFFFFFFFF;
		TOffset.FileIndex = 0xFFFFFFFF;
	#endif
	}
	else
	{
		TOffset.Pointer = (uint64_t) NextEntry;
		TOffset.FileIndex++;
	}
	
	Iosb->ReadDir.NextOffset = TOffset.Offset;
	Iosb->ReadDir.Version = Ext->Version;
	
	PTMPFS_EXT ChildExt = EXT(DirEntry->Fcb);
	
	memcpy(Entry->Name, DirEntry->Name, IO_MAX_NAME);
	Entry->InodeNumber = ChildExt->InodeNumber;
	Entry->Type = DirEntry->Fcb->FileType;
	
	KeReleaseMutex(&Ext->Lock);
	return IOSB_STATUS(Iosb, STATUS_SUCCESS);
}

BSTATUS TmpParseDir(PIO_STATUS_BLOCK Iosb, PFILE_OBJECT FileObject, const char* ParsePath, UNUSED int ParseLoopCount)
{
	PFCB InitialFcb = FileObject->Fcb;
	
	if (InitialFcb->FileType == FILE_TYPE_DIRECTORY)
	{
		if (*ParsePath == '\0' || *ParsePath == OB_PATH_SEPARATOR)
			goto ParsedThis;
		
		// This is a directory, so let's read it until we find something.
		BSTATUS Status;
		PTMPFS_EXT Ext;
		
		Ext = EXT(InitialFcb);
		Status = KeWaitForSingleObject(&Ext->Lock, true, TMPFS_TIMEOUT, KeGetPreviousMode());
		if (FAILED(Status))
			return IOSB_STATUS(Iosb, Status);
		
		for (PLIST_ENTRY ListEntry = Ext->ChildrenListHead.Flink;
			ListEntry != &Ext->ChildrenListHead;
			ListEntry = ListEntry->Flink)
		{
			PTMPFS_DIR_ENTRY DirEntry = CONTAINING_RECORD(ListEntry, TMPFS_DIR_ENTRY, ListEntry);
			
			size_t Match = ObMatchPathName(DirEntry->Name, ParsePath);
			if (!Match)
				continue;
			
			// Path name matched, so return the new FCB.
			ASSERT(DirEntry->ParentFcb == InitialFcb);
			
			TmpReferenceFcb(DirEntry->Fcb);
			Iosb->ParseDir.FoundFcb = DirEntry->Fcb;
			Iosb->ParseDir.ReparsePath = ParsePath + Match;
			KeReleaseMutex(&Ext->Lock);
			return IOSB_STATUS(Iosb, STATUS_SUCCESS);
		}
		
		// Not Found
		KeReleaseMutex(&Ext->Lock);
		return IOSB_STATUS(Iosb, STATUS_NAME_NOT_FOUND);
	}
	else if (InitialFcb->FileType == FILE_TYPE_SYMBOLIC_LINK)
	{
		// TODO
		ASSERT(!"Symlink Parse Not Implemented");
		return IOSB_STATUS(Iosb, STATUS_UNIMPLEMENTED);
	}
	else
	{
		// This is a file or something else, so only return success
		// if there are no remaining path components remaining.
		if (!ParsePath || *ParsePath == '\0')
		{
		ParsedThis:
			Iosb->ParseDir.FoundFcb = InitialFcb;
			Iosb->ParseDir.ReparsePath = NULL;
			TmpReferenceFcb(InitialFcb);
			return IOSB_STATUS(Iosb, STATUS_SUCCESS);
		}
		
		return IOSB_STATUS(Iosb, STATUS_NOT_A_DIRECTORY);
	}
}

BSTATUS TmpUnlink(PFCB ContainingFcb, bool IsEntryFromReadDir, PIO_DIRECTORY_ENTRY DirectoryEntry)
{
	PTMPFS_EXT Ext = EXT(ContainingFcb);
	
	// NOTE: An FCB cannot transmute itself (i.e. become a directory from a non-directory).
	if (ContainingFcb->FileType != FILE_TYPE_DIRECTORY)
		return STATUS_NOT_A_DIRECTORY;
	
	BSTATUS Status = KeWaitForSingleObject(&Ext->Lock, true, TMPFS_TIMEOUT, KeGetPreviousMode());
	if (FAILED(Status))
		return Status;
	
	(void) IsEntryFromReadDir;

	PTMPFS_DIR_ENTRY Target = NULL;
	
	for (PLIST_ENTRY ListEntry = Ext->ChildrenListHead.Flink;
		ListEntry != &Ext->ChildrenListHead;
		ListEntry = ListEntry->Flink)
	{
		PTMPFS_DIR_ENTRY DirEntry = CONTAINING_RECORD(ListEntry, TMPFS_DIR_ENTRY, ListEntry);
		
		if (strcmp(DirEntry->Name, DirectoryEntry->Name) == 0)
		{
			Target = DirEntry;
			break;
		}
	}
	
	if (!Target)
	{
		KeReleaseMutex(&Ext->Lock);
		return STATUS_NAME_NOT_FOUND;
	}
	
	if (Target->Fcb->FileType == FILE_TYPE_DIRECTORY)
	{
		// The target is a directory, we may not remove it using this call and must
		// use RemoveDir instead.
		KeReleaseMutex(&Ext->Lock);
		return STATUS_IS_A_DIRECTORY;
	}
	
	RemoveEntryList(&Target->ListEntry);
	TmpDereferenceFcb(Target->ParentFcb);
	TmpDereferenceFcb(Target->Fcb);
	MmFreePool(Target);
	
	KeReleaseMutex(&Ext->Lock);
	return STATUS_SUCCESS;
}

BSTATUS TmpRemoveDir(PFCB Fcb)
{
	PREP_EXT;
	
	if (Fcb->FileType != FILE_TYPE_DIRECTORY)
		return STATUS_NOT_A_DIRECTORY;
	
	BSTATUS Status = KeWaitForSingleObject(&Ext->Lock, true, TMPFS_TIMEOUT, KeGetPreviousMode());
	if (FAILED(Status))
		return Status;
	
	// Check if this directory is parent-less (root directory).
	if (Ext->ParentEntry == NULL)
	{
		KeReleaseMutex(&Ext->Lock);
		return STATUS_DIRECTORY_NOT_EMPTY;
	}
	
	// Check if this directory has any entries other than "." or "..".
	PTMPFS_DIR_ENTRY DotEntry = NULL, DotDotEntry = NULL;
	
	for (PLIST_ENTRY ListEntry = Ext->ChildrenListHead.Flink;
		ListEntry != &Ext->ChildrenListHead;
		ListEntry = ListEntry->Flink)
	{
		PTMPFS_DIR_ENTRY DirEntry = CONTAINING_RECORD(ListEntry, TMPFS_DIR_ENTRY, ListEntry);
		
		if (strcmp(DirEntry->Name, ".") == 0 && !DotEntry)
		{
			DotEntry = DirEntry;
			continue;
		}
		
		if (strcmp(DirEntry->Name, "..") == 0 && !DotDotEntry)
		{
			DotDotEntry = DirEntry;
			continue;
		}
		
		// Neither "." nor "..", so directory not empty.
		KeReleaseMutex(&Ext->Lock);
		return STATUS_DIRECTORY_NOT_EMPTY;
	}
	
	if (!DotDotEntry)
	{
		// This shouldn't be possible.
		DbgPrint("TmpRemoveDir: No dot dot entry?!");
		KeReleaseMutex(&Ext->Lock);
		return STATUS_DIRECTORY_NOT_EMPTY;
	}
	
	PFCB ParentFcb = DotDotEntry->Fcb;
	PREP_EXT_PARENT;
	
	// This was checked above, because when the ".." entry redirects to the same FCB,
	// this directory has no parent.
	ASSERT(ParentFcb != Fcb);
	
	Status = KeWaitForSingleObject(&ParentExt->Lock, true, TMPFS_TIMEOUT, KeGetPreviousMode());
	if (FAILED(Status))
	{
		DbgPrint("TmpRemoveDir: Failed to acquire parent mutex: %s (%d)", RtlGetStatusString(Status), Status);
		KeReleaseMutex(&Ext->Lock);
		return Status;
	}
	
	// Remove the parent's entry.
	RemoveEntryList(&Ext->ParentEntry->ListEntry);
	TmpDereferenceFcb(Ext->ParentEntry->ParentFcb);
	TmpDereferenceFcb(Ext->ParentEntry->Fcb);
	MmFreePool(Ext->ParentEntry);
	
	// Free our entries.
	TmpDereferenceFcb(DotEntry->ParentFcb);
	TmpDereferenceFcb(DotEntry->Fcb);
	TmpDereferenceFcb(DotDotEntry->ParentFcb);
	TmpDereferenceFcb(DotDotEntry->Fcb);
	MmFreePool(DotEntry);
	MmFreePool(DotDotEntry);
	
	KeReleaseMutex(&ParentExt->Lock);
	KeReleaseMutex(&Ext->Lock);
	return STATUS_SUCCESS;
}

BSTATUS TmpMakeLink(PFCB Fcb, PIO_DIRECTORY_ENTRY NewName, PFCB DestFcb)
{
	if (DestFcb->FileType == FILE_TYPE_DIRECTORY)
		return STATUS_IS_A_DIRECTORY;
	
	PREP_EXT;
	if (Fcb->FileType != FILE_TYPE_DIRECTORY)
		return STATUS_NOT_A_DIRECTORY;
	
	PTMPFS_DIR_ENTRY DirEntry = MmAllocatePool(POOL_NONPAGED, sizeof(TMPFS_DIR_ENTRY));
	if (!DirEntry)
		return STATUS_INSUFFICIENT_MEMORY;
	
	BSTATUS Status = KeWaitForSingleObject(&Ext->Lock, true, TMPFS_TIMEOUT, KeGetPreviousMode());
	if (FAILED(Status))
	{
		MmFreePool(DirEntry);
		return Status;
	}
	
	DirEntry->ParentFcb = Fcb;
	DirEntry->Fcb = DestFcb;
	TmpReferenceFcb(DirEntry->ParentFcb);
	TmpReferenceFcb(DirEntry->Fcb);
	
	memcpy(DirEntry->Name, NewName->Name, IO_MAX_NAME);
	InsertTailList(&Ext->ChildrenListHead, &DirEntry->ListEntry);
	
	KeReleaseMutex(&Ext->Lock);
	return STATUS_SUCCESS;
}

IO_DISPATCH_TABLE TmpDispatchTable =
{
	.Seekable = TmpFileSeekable,
	.Reference = TmpReferenceFcb,
	.Dereference = TmpDereferenceFcb,
	.BackingMemory = TmpBackingMemory,
	.Read = TmpRead,
	.Write = TmpWrite,
	.Resize = TmpResize,
	.MakeFile = TmpMakeFile,
	.MakeDir = TmpMakeDir,
	.MakeLink = TmpMakeLink,
	.Unlink = TmpUnlink,
	.RemoveDir = TmpRemoveDir,
	.ReadDir = TmpReadDir,
	.ParseDir = TmpParseDir,
};

// ** Done with FCB dispatch functions **

BSTATUS TmpCreateFile(PFILE_OBJECT* OutFileObject, void* InitialAddress, size_t InitialLength, uint8_t FileType, PFCB ParentFcb, const char* Name)
{
	BSTATUS Status;
	PFCB Fcb = IoAllocateFcb(&TmpDispatchTable, sizeof(TMPFS_EXT), true);
	if (!Fcb)
		return STATUS_INSUFFICIENT_MEMORY;
	
	Fcb->FileType = FileType;
	Fcb->FileLength = InitialLength;
	
	PREP_EXT;
	KeInitializeMutex(&Ext->Lock, 0);
	Ext->ReferenceCount = 1;
	Ext->InitialAddress = InitialAddress;
	Ext->InitialLength = InitialLength;
	Ext->InodeNumber = AtAddFetch(NextTmpfsInodeNumber, 1);
	Ext->PageList = NULL;
	Ext->PageListCount = 0;
	
	// If InitialLength is set, then InitialAddress must also be set.
	ASSERT(Ext->InitialLength ? (Ext->InitialAddress != NULL) : true);
	
	InitializeListHead(&Ext->ChildrenListHead);
	
	// Create a file object for it.
	PFILE_OBJECT FileObject;
	Status = IoCreateFileObject(Fcb, &FileObject, 0, 0);
	if (FAILED(Status))
	{
		IoFreeFcb(Fcb);
		return Status;
	}
	
	// If there is a parent FCB, add this FCB to the parent.
	PTMPFS_DIR_ENTRY DirEntry = NULL;
	if (ParentFcb)
	{
		TmpReferenceFcb(ParentFcb);
		TmpReferenceFcb(Fcb);
		
		DirEntry = MmAllocatePool(POOL_NONPAGED, sizeof(TMPFS_DIR_ENTRY));
		if (!DirEntry)
		{
			TmpDereferenceFcb(ParentFcb);
			TmpDereferenceFcb(Fcb);
			ObDereferenceObject(FileObject);
			IoFreeFcb(Fcb);
			return STATUS_INSUFFICIENT_MEMORY;
		}
		
		DirEntry->ParentFcb = ParentFcb;
		DirEntry->Fcb = Fcb;
		
		strncpy(DirEntry->Name, Name, sizeof DirEntry->Name);
		DirEntry->Name[sizeof DirEntry->Name - 1] = 0;
		
		PREP_EXT_PARENT;
		Status = KeWaitForSingleObject(&ParentExt->Lock, false, TMPFS_TIMEOUT, MODE_KERNEL);
		ASSERT(SUCCEEDED(Status));
		
		InsertTailList(&ParentExt->ChildrenListHead, &DirEntry->ListEntry);
		KeReleaseMutex(&ParentExt->Lock);
	}
	
	// If this is a directory, add the "." and ".." entries.
	if (FileType == FILE_TYPE_DIRECTORY)
	{
		Ext->ParentEntry = DirEntry;
		
		PTMPFS_DIR_ENTRY DotDirEntry, DotDotDirEntry;
		
		DotDirEntry = MmAllocatePool(POOL_NONPAGED, sizeof(TMPFS_DIR_ENTRY));
		DotDotDirEntry = MmAllocatePool(POOL_NONPAGED, sizeof(TMPFS_DIR_ENTRY));
		
		if (!DotDirEntry || !DotDotDirEntry)
		{
			if (DotDirEntry) MmFreePool(DotDirEntry);
			if (DotDotDirEntry) MmFreePool(DotDotDirEntry);
			
			if (ParentFcb)
			{
				RemoveEntryList(&DirEntry->ListEntry);
				MmFreePool(DirEntry);
				TmpDereferenceFcb(ParentFcb);
			}
			
			ObDereferenceObject(FileObject);
			IoFreeFcb(Fcb);
			return STATUS_INSUFFICIENT_MEMORY;
		}
		
		strcpy(DotDirEntry->Name, ".");
		strcpy(DotDotDirEntry->Name, "..");
		
		// Both entries belong to this FCB.
		DotDirEntry->ParentFcb = Fcb;
		DotDotDirEntry->ParentFcb = Fcb;
		DotDirEntry->Fcb = Fcb;
		DotDotDirEntry->Fcb = ParentFcb ? ParentFcb : Fcb;
		
		// Add references to all of them.
		TmpReferenceFcb(DotDirEntry->ParentFcb);
		TmpReferenceFcb(DotDotDirEntry->ParentFcb);
		TmpReferenceFcb(DotDirEntry->Fcb);
		TmpReferenceFcb(DotDotDirEntry->Fcb);
	}
	
	*OutFileObject = FileObject;
	return Status;
}
