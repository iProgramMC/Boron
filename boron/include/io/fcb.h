/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/fcb.h
	
Abstract:
	This header defines the I/O manager's part of the
	File Control Block structure.  The I/O manager takes
	a relatively hands-off approach to file control, as
	can be seen by the relatively few fields its FCB
	struct features.  Control over management is instead
	surrendered to the File System Driver (FSD).
	
Author:
	iProgramInCpp - 22 June 2024
***/
#pragma once

#include <ex/rwlock.h>
#include <mm/cache.h>
#include <io/dispatch.h>

// Supported types of files:
enum FILE_TYPE
{
	FILE_TYPE_UNKNOWN,
	FILE_TYPE_FILE,
	FILE_TYPE_DIRECTORY,
	FILE_TYPE_BLOCK_DEVICE,
	FILE_TYPE_CHARACTER_DEVICE,
};

typedef struct _FCB
{
	PDEVICE_OBJECT DeviceObject;
	
	PIO_DISPATCH_TABLE DispatchTable;
	
	EX_RW_LOCK RwLock;
	
	CCB PageCache;
	
	RBTREE ViewCache;
	KMUTEX ViewCacheMutex;
	
	// FILE_TYPE
	uint8_t FileType;
	uint32_t Flags;
	
	// Valid only for files and block devices.  Otherwise it's zero.
	uint64_t FileLength;
	
	// FSD specific extension.  When the FCB is initialized, the
	// size of this extension may be specified.
	size_t ExtensionSize;
	char Extension[];
}
FCB, *PFCB;

// Allocates an FCB.  
PFCB IoAllocateFcb(PDEVICE_OBJECT DeviceObject, size_t ExtensionSize, bool NonPaged);

// Frees an FCB.
void IoFreeFcb(PFCB Fcb);

// Dereferences an FCB.
//
// This calls its Dereference function.  The Boron executive does not store the
// reference count of an FCB.
//
// (The FCB's reference count is increased every time IO_CREATE_OBJ_METHOD is called)
void IoDereferenceFcb(PFCB Fcb);

// TODO: Maybe the waits should be alertable?
// Locks an FCB exclusive.
static ALWAYS_INLINE inline BSTATUS IoLockFcbExclusive(PFCB Fcb)
{
	return ExAcquireExclusiveRwLock(&Fcb->RwLock, false, false);
}

// Locks an FCB shared.
static ALWAYS_INLINE inline BSTATUS IoLockFcbShared(PFCB Fcb)
{
	return ExAcquireSharedRwLock(&Fcb->RwLock, false, false, false);
}

// Unlocks an FCB.
static ALWAYS_INLINE inline void IoUnlockFcb(PFCB Fcb)
{
	ExReleaseRwLock(&Fcb->RwLock);
}

// Demotes an FCB's lock mode to shared.
static ALWAYS_INLINE inline void IoDemoteToSharedFcb(PFCB Fcb)
{
	ExDemoteToSharedRwLock(&Fcb->RwLock);
}

static ALWAYS_INLINE inline bool IoIsDirectlyMappable(PFCB Fcb)
{
	return Fcb->DispatchTable->Flags & DISPATCH_FLAG_DIRECTLY_MAPPABLE;
}
