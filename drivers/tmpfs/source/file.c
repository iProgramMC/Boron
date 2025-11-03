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

#define PREP_EXT PTMPFS_EXT Ext = (PTMPFS_EXT) Fcb->Extension
#define PREP_EXT_PARENT PTMPFS_EXT ParentExt = (PTMPFS_EXT) ParentFcb->Extension

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

IO_DISPATCH_TABLE TmpDispatchTable =
{
    .Seekable = TmpFileSeekable,
    .Reference = TmpReferenceFcb,
    .Dereference = TmpDereferenceFcb,
    //...
};

BSTATUS TmpCreateFile(PFILE_OBJECT* OutFileObject, void* InitialAddress, size_t InitialLength, uint8_t FileType, PFCB ParentFcb, const char* Name)
{
    BSTATUS Status;
    PFCB Fcb = IoAllocateFcb(&TmpDispatchTable, sizeof(TMPFS_EXT), true);
    if (!Fcb)
        return STATUS_INSUFFICIENT_MEMORY;
    
    PREP_EXT;
    KeInitializeMutex(&Ext->Lock, 0);
    Ext->FileType = FileType;
    Ext->ReferenceCount = 1;
    Ext->InitialAddress = InitialAddress;
    Ext->InitialLength = InitialLength;
    Ext->FileLength = InitialLength;
    
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
    if (ParentFcb)
    {
        TmpReferenceFcb(ParentFcb);
        TmpReferenceFcb(Fcb);
        
        PTMPFS_DIR_ENTRY DirEntry = MmAllocatePool(POOL_NONPAGED, sizeof(TMPFS_DIR_ENTRY));
        if (!DirEntry)
        {
            TmpDereferenceFcb(ParentFcb);
            TmpDereferenceFcb(Fcb);
            IoFreeFcb(Fcb);
            ObDereferenceObject(FileObject);
            return STATUS_INSUFFICIENT_MEMORY;
        }
        
        DirEntry->ParentFcb = ParentFcb;
        DirEntry->Fcb = Fcb;
        
        strncpy(DirEntry->Name, Name, sizeof DirEntry->Name);
        DirEntry->Name[sizeof DirEntry->Name - 1] = 0;
        
        PREP_EXT_PARENT;
        Status = KeWaitForSingleObject(&ParentExt->Lock, false, TIMEOUT_INFINITE, MODE_KERNEL);
        ASSERT(SUCCEEDED(Status));
        
        InsertTailList(&ParentExt->ChildrenListHead, &DirEntry->ListEntry);
        KeReleaseMutex(&ParentExt->Lock);
    }
    
    *OutFileObject = FileObject;
    return Status;
}
