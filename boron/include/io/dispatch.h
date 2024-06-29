/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/dispatch.h
	
Abstract:
	This header file defines the I/O dispatch table used by
	driver objects.
	
Author:
	iProgramInCpp - 29 June 2024
***/
#pragma once

#include "iosb.h"
#include "dirent.h"

//
// Notes:
//
// - The "Block" parameter being false means that the operation may not block.
//
// - The BSTATUS returned must have the same value as the value inside the "Status" pointer, if it exists.
//
// - If IsEntryFromReadDir is TRUE, IO_LOOKUP_DIR_METHOD, IO_UNLINK_METHOD and IO_REMOVE_DIR_METHOD may optimize by opening
//   the inode/file according to the other fields of the IO_DIRECTORY_ENTRY, instead of performing a potentially expensive
//   lookup again on the name.  If it is false, the other fields must be IGNORED and the file name must be taken into
//   account.
//
// - In IO_MAKE_FILE_METHOD, IO_MAKE_DIRECTORY_METHOD, and IO_MAKE_LINK_METHOD, anything but IO_DIRECTORY_ENTRY->Name is
//   ignored.
//
// - IO_REMOVE_DIR_METHOD removes an empty directory by unlinking it from the parent.  Requires special processing, because
//   on some file systems, such as ext2, the link count of the parent and child directory's inodes is biased by 1 due to
//   their cyclic dependency.
//
// - IO_MOVE_ENTRY_METHOD moves a directory entry from the source to the destination directory.  If NewEntry is NULL, then
//   the old directory entry's name is used as the new directory entry's name.  NewEntry fields other than Name are ignored.
//   See the third note for details about IsOldEntryFromReadDir.
//
// - For IO_CHANGE_MODE_METHOD, the mode of the file isn't specified for now.
//
// - For IO_CHANGE_TIME_METHOD, if one of the time fields is not to be modified, pass 0.
//
// - IO_CREATE_METHOD is the first dispatch call performed on an FCB.  Here, the file system driver may fill in initial
//   details about the FCB.
//
// - IO_DELETE_METHOD is the final dispatch call performed on an FCB.  After the call, the FCB is deleted from the host
//   file system's cache.
//
// - The initial set of calls have been copied from NanoShell as they have been deemed adequate.  More calls might be added
//   in the future, or these calls might be changed.
//
typedef BSTATUS(*IO_CREATE_METHOD)     (PFCB Fcb, void* Context);
typedef void   (*IO_DELETE_METHOD)     (PFCB Fcb);
typedef BSTATUS(*IO_OPEN_METHOD)       (PFCB Fcb, int OpenFlags);
typedef BSTATUS(*IO_CLOSE_METHOD)      (PFCB Fcb);
typedef BSTATUS(*IO_READ_METHOD)       (PIO_STATUS_BLOCK Status, PFCB Fcb, uintptr_t Offset, size_t Length, void* Buffer, bool Block);
typedef BSTATUS(*IO_WRITE_METHOD)      (PIO_STATUS_BLOCK Status, PFCB Fcb, uintptr_t Offset, size_t Length, void* Buffer, bool Block);
typedef BSTATUS(*IO_OPEN_DIR_METHOD)   (PFCB Fcb);
typedef BSTATUS(*IO_CLOSE_DIR_METHOD)  (PFCB Fcb);
typedef BSTATUS(*IO_READ_DIR_METHOD)   (PIO_STATUS_BLOCK Status, PFCB Fcb, uintptr_t Offset, PIO_DIRECTORY_ENTRY DirectoryEntry);
typedef BSTATUS(*IO_LOOKUP_DIR_METHOD) (PIO_STATUS_BLOCK Status, PFCB Fcb, bool IsEntryFromReadDir, PIO_DIRECTORY_ENTRY DirectoryEntry);
typedef BSTATUS(*IO_RESIZE_METHOD)     (PFCB Fcb, size_t NewLength);
typedef BSTATUS(*IO_MAKE_FILE_METHOD)  (PFCB ContainingFcb, PIO_DIRECTORY_ENTRY Name);
typedef BSTATUS(*IO_MAKE_DIR_METHOD)   (PFCB ContainingFcb, PIO_DIRECTORY_ENTRY Name);
typedef BSTATUS(*IO_UNLINK_METHOD)     (PFCB ContainingFcb, bool IsEntryFromReadDir, PIO_DIRECTORY_ENTRY DirectoryEntry);
typedef BSTATUS(*IO_REMOVE_DIR_METHOD) (PFCB Fcb);
typedef BSTATUS(*IO_MOVE_ENTRY_METHOD) (PFCB SourceDirFcb, PFCB DestDirFcb, bool IsOldEntryFromReadDir, PIO_DIRECTORY_ENTRY OldEntry, PIO_DIRECTORY_ENTRY NewEntry);
typedef BSTATUS(*IO_IO_CONTROL_METHOD) (PFCB Fcb, int IoControlCode, const void* InBuffer, size_t InBufferSize, void* OutBuffer, size_t OutBufferSize);
typedef BSTATUS(*IO_CHANGE_MODE_METHOD)(PFCB Fcb, uintptr_t NewMode);
typedef BSTATUS(*IO_CHANGE_TIME_METHOD)(PFCB Fcb, uintptr_t CreateTime, uintptr_t ModifyTime, uintptr_t AccessTime);
typedef BSTATUS(*IO_MAKE_LINK_METHOD)  (PFCB Fcb, PIO_DIRECTORY_ENTRY NewName, PFCB DestinationFile);

typedef struct _IO_DISPATCH_TABLE
{
	IO_CREATE_METHOD      Create;
	IO_DELETE_METHOD      Delete;
	IO_OPEN_METHOD        Open;
	IO_CLOSE_METHOD       Close;
	IO_READ_METHOD        Read;
	IO_WRITE_METHOD       Write;
	IO_OPEN_DIR_METHOD    OpenDir;
	IO_CLOSE_DIR_METHOD   CloseDir;
	IO_READ_DIR_METHOD    ReadDir;
	IO_LOOKUP_DIR_METHOD  LookupDir;
	IO_RESIZE_METHOD      Resize;
	IO_MAKE_FILE_METHOD   MakeFile;
	IO_MAKE_DIR_METHOD    MakeDir;
	IO_UNLINK_METHOD      Unlink;
	IO_REMOVE_DIR_METHOD  RemoveDir;
	IO_MOVE_ENTRY_METHOD  MoveEntry;
	IO_IO_CONTROL_METHOD  IoControl;
	IO_CHANGE_MODE_METHOD ChangeMode;
	IO_CHANGE_TIME_METHOD ChangeTime;
	IO_MAKE_LINK_METHOD   MakeLink;
}
IO_DISPATCH_TABLE, *PIO_DISPATCH_TABLE;