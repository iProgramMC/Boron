/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ext2.h
	
Abstract:
	This header defines all of the structures used by
	the ext2 file system driver as well as the ext2
	I/O dispatch function prototypes.
	
Author:
	iProgramInCpp - 15 May 2025
***/
#pragma once

#include <main.h>
#include <io.h>
#include <ob.h>
#include <mm.h>
#include <cc.h>

#include "dskstrct.h"

// Forward type definitions.
typedef struct _EXT2_FILE_SYSTEM EXT2_FILE_SYSTEM, *PEXT2_FILE_SYSTEM;

// The FCB extension of an ext2 inode.
typedef struct _EXT2_FCB_EXT
{
	uint32_t InodeNumber;
	
	EXT2_INODE Inode;
	
	PEXT2_FILE_SYSTEM OwnerFS;
	
	bool AddedToInodeTree;
	RBTREE_ENTRY InodeTreeEntry;
	
	uintptr_t ReferenceCount;
}
EXT2_FCB_EXT, *PEXT2_FCB_EXT;

#define EXT(Fcb) ((PEXT2_FCB_EXT)(Fcb->Extension))
#define FCB(Ext) CONTAINING_RECORD((Ext), FCB, Extension)

PFCB Ext2CreateFcb(PEXT2_FILE_SYSTEM FileSystem, uint32_t InodeNumber);
void Ext2FreeInode(PFCB Fcb);

// ** FCB Dispatch Functions **

extern IO_DISPATCH_TABLE Ext2DispatchTable;

BSTATUS Ext2Mount(PDEVICE_OBJECT, PFILE_OBJECT);

void Ext2DereferenceInode(PFCB Fcb);


// *** Ext2 File System Struct ***

// The file system instance.  Has a reference for every in-memory inode.
struct _EXT2_FILE_SYSTEM
{
	PDEVICE_OBJECT BackingDevice;
	PFILE_OBJECT File;
	
	EXT2_SUPERBLOCK SuperBlock;
	
	// If this file system is read only because it uses features that make it so.
	bool ForceReadOnly;
	
	// Some pre-calculated items.
	int FirstInode;
	int InodeSize;
	int BlockSize;
	int FragmentSize;
	int InodesPerGroup;
	int BlocksPerGroup;
	int BlockSizeLog2;
	
	// This is for lookup by inode #.
	RBTREE InodeTree;
	KMUTEX InodeTreeMutex;
};

// ** Ext2 File System object type functions **
extern POBJECT_TYPE Ext2FileSystemType;

void Ext2DeleteFileSystem(void* FileSystemV);

BSTATUS Ext2CreateFileSystemObject(PEXT2_FILE_SYSTEM* Out);

BSTATUS Ext2OpenInode(PEXT2_FILE_SYSTEM FileSystem, uint32_t InodeNumber, PFCB* OutFcb);
