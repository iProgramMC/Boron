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
// - TODO: Define a kernel-wide time format (64-bit Unix epoch time?)
//
// - IO_BACKING_MEM_METHOD gets the backing memory address and length from the FCB.  This is usually not implemented for
//   on-disk file systems, but can be implemented for RAM-backed file systems and certain MMIO devices, such as frame buffer
//   devices.
//
// - IO_CREATE_OBJ_METHOD is the method called when a file object is created from this FCB.
//   Note: This method will not add a reference to the FCB.  The job of adding a reference to the FCB falls on the FSD
//   (File system driver) in methods that return an FCB, such as IO_PARSE_METHOD.
//
// - IO_DELETE_OBJ_METHOD is the method called when a file object is deleted with a reference to this FCB.
//   This does not erase the reference to the FCB, but the file object, when deleted, will also separately call
//   IO_DEREFERENCE_METHOD.
//
// - IO_DEREFERENCE_METHOD is the method called when a reference to the FCB is deleted. For example, a file object being
//   deleted will call this function on its FCB.
//
// - Careful! IO_READ_METHOD and IO_WRITE_METHOD may be called from the context of multiple threads at the same time!
//   Device and FS drivers must take care to prevent race conditions in this regard.  Note that this may be disabled
//   by setting a flag in the dispatch table.
//
// - For IO_WRITE_METHOD, IsLockedExclusively is used when determining whether to drop the FCB's rwlock and re-acquire
//   it exclusively for expansion.
//
// - The IO_TOUCH_METHOD method lets the FSD know that a file was updated and that the relevant time stamp (modify or
//   access) should be updated.
//
// - IO_PARSE_DIR_METHOD, in the Iosb's ReparsePath, will ONLY return NULL or a substring of ParsePath.
//
typedef BSTATUS(*IO_CREATE_METHOD)     (PFCB Fcb, void* Context);
typedef void   (*IO_CREATE_OBJ_METHOD) (PFCB Fcb, void* FileObject);
typedef void   (*IO_DELETE_METHOD)     (PFCB Fcb);
typedef void   (*IO_DELETE_OBJ_METHOD) (PFCB Fcb, void* FileObject);
typedef void   (*IO_DEREFERENCE_METHOD)(PFCB Fcb);
typedef BSTATUS(*IO_OPEN_METHOD)       (PFCB Fcb, uint32_t OpenFlags);
typedef BSTATUS(*IO_CLOSE_METHOD)      (PFCB Fcb, int LastHandleCount);
typedef BSTATUS(*IO_READ_METHOD)       (PIO_STATUS_BLOCK Iosb, PFCB Fcb, uintptr_t Offset, size_t Length, void* Buffer, bool Block);
typedef BSTATUS(*IO_WRITE_METHOD)      (PIO_STATUS_BLOCK Iosb, PFCB Fcb, uintptr_t Offset, size_t Length, const void* Buffer, bool Block, bool IsLockedExclusively);
typedef BSTATUS(*IO_OPEN_DIR_METHOD)   (PFCB Fcb);
typedef BSTATUS(*IO_CLOSE_DIR_METHOD)  (PFCB Fcb);
typedef BSTATUS(*IO_READ_DIR_METHOD)   (PIO_STATUS_BLOCK Iosb, PFCB Fcb, uintptr_t Offset, PIO_DIRECTORY_ENTRY DirectoryEntry);
typedef BSTATUS(*IO_PARSE_DIR_METHOD)  (PIO_STATUS_BLOCK Iosb, PFCB InitialFcb, const char* ParsePath, int ParseLoopCount);
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
typedef BSTATUS(*IO_TOUCH_METHOD)      (PFCB Fcb, bool IsWrite);
typedef BSTATUS(*IO_BACKING_MEM_METHOD)(PIO_STATUS_BLOCK Iosb, PFCB Fcb);

enum
{
	// If this flag is set, the FCB's rwlock will function like a mutex and will
	// keep the Read and Write functions from being called in different threads.
	//
	// If this flag is NOT set, the FCB's rwlock has the purpose of preventing the
	// file from being resized while the read/write operation is being performed.
	// While resizing the file through IO_RESIZE_METHOD, the I/O manager will
	// acquire the FCB's rwlock exclusive.  For example, in a write operation, if
	// the file is to be expanded, the write dispatch function will drop the rw-lock
	// and re-lock it as exclusive, expand the file, convert it back to shared,
	// and commit the write again.
	//
	// As an optimization, the FCB's rwlock, when the file is opened as append only
	// and it's written to, will start out locked as exclusive.
	//
	// This does not affect operations other than `Read` and `Write`.
	DISPATCH_FLAG_EXCLUSIVE = (1 << 0),
};

typedef struct _IO_DISPATCH_TABLE
{
	uint32_t              Flags;
	IO_CREATE_METHOD      Create;
	IO_DELETE_METHOD      Delete;
	IO_CREATE_OBJ_METHOD  CreateObject;
	IO_DELETE_OBJ_METHOD  DeleteObject;
	IO_DEREFERENCE_METHOD Dereference;
	IO_OPEN_METHOD        Open;
	IO_CLOSE_METHOD       Close;
	IO_READ_METHOD        Read;
	IO_WRITE_METHOD       Write;
	IO_OPEN_DIR_METHOD    OpenDir;
	IO_CLOSE_DIR_METHOD   CloseDir;
	IO_READ_DIR_METHOD    ReadDir;
	IO_PARSE_DIR_METHOD   ParseDir;
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
	IO_TOUCH_METHOD       Touch;
	IO_BACKING_MEM_METHOD BackingMemory;
}
IO_DISPATCH_TABLE, *PIO_DISPATCH_TABLE;