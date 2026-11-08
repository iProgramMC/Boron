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
#include <string.h>
#include <io.h>
#include <ob.h>
#include <mm.h>
#include <cc.h>
#include <ex.h>

#include "dskstrct.h"

// Forward type definitions.
typedef struct _EXT2_FILE_SYSTEM EXT2_FILE_SYSTEM, *PEXT2_FILE_SYSTEM;

// The FCB extension of an ext2 inode.
typedef struct _EXT2_FCB_EXT
{
	// This inode's number.
	uint32_t InodeNumber;

	// This inode's reference count.
	uintptr_t ReferenceCount;

	// The on-disk representation of the file.
	//
	// TODO: Use the mutex in more places.
	FAST_MUTEX InodeDataMutex;
	EXT2_INODE Inode;
	bool InodeDataIsDirty;
	uint64_t InodeWritebackAddress;

	// This guards the direct block array.
	EX_RW_LOCK BlockRwlock;

	// Back-pointer to the file system instance.
	PEXT2_FILE_SYSTEM OwnerFS;

	// This FCB's location in the file system's inode tree.
	bool AddedToInodeTree;
	RBTREE_ENTRY InodeTreeEntry;

	// This field tells us whether or not we need to re-validate
	// when performing a "read directory" operation.
	uint64_t Version;
}
EXT2_FCB_EXT, *PEXT2_FCB_EXT;

#define EXT(Fcb) ((PEXT2_FCB_EXT)(Fcb->Extension))
#define FCB(Ext) CONTAINING_RECORD((Ext), FCB, Extension)

PFCB Ext2CreateFcb(PEXT2_FILE_SYSTEM FileSystem, uint32_t InodeNumber);
void Ext2FreeInode(PFCB Fcb);

// ** FCB Dispatch Functions **

extern IO_DISPATCH_TABLE Ext2DispatchTable;

BSTATUS Ext2Mount(PDEVICE_OBJECT, PFILE_OBJECT, POBJECT_DIRECTORY MountDir);

void Ext2CreateObject(PFCB Fcb, PFILE_OBJECT FileObject);

void Ext2ReferenceInode(PFCB Fcb);

void Ext2DereferenceInode(PFCB Fcb);

bool Ext2Seekable(PFCB Fcb);

BSTATUS Ext2TouchFile(PFCB Fcb, bool IsWrite);

BSTATUS Ext2ResizeFile(PFCB Fcb, uint64_t NewLength);

BSTATUS Ext2Read(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, uint32_t Flags);

BSTATUS Ext2ReadDir(PIO_STATUS_BLOCK Iosb, PFILE_OBJECT FileObject, uint64_t Offset, uint64_t Version, PIO_DIRECTORY_ENTRY DirectoryEntry);

BSTATUS Ext2ParseDir(PIO_STATUS_BLOCK Iosb, PFILE_OBJECT FileObject, const char* ParsePath, UNUSED int ParseLoopCount, PIO_PARSE_CONTEXT ParseContext);

BSTATUS Ext2FlushMetadata(PFCB Fcb);

// *** Ext2 File System Struct ***

// The file system instance.  Has a reference for every in-memory inode.
struct _EXT2_FILE_SYSTEM
{
	PDEVICE_OBJECT BackingDevice;
	PFILE_OBJECT File;

	// TODO: Use super block APIs instead of poking directly into the superblock.
	//
	// NOTE: We use a spin lock here because the super block is high traffic but operations
	// on the super block are very simple.
	KSPIN_LOCK SuperBlockSpinLock;
	EXT2_SUPERBLOCK SuperBlock;
	bool SuperBlockIsDirty;

	// If this file system is read only because it uses features that make it so.
	bool ForceReadOnly;

	// Some pre-calculated items.
	uint32_t FirstInode;
	uint32_t InodeSize;
	uint32_t BlockSize;
	uint32_t FragmentSize;
	uint32_t InodesPerGroup;
	uint32_t BlocksPerGroup;
	uint32_t BlockSizeLog2;

	// This is for lookup by inode #.
	RBTREE InodeTree;
	FAST_MUTEX InodeTreeMutex;

	// This mutex is for resource allocation.
	FAST_MUTEX BlockAllocationMutex;
	FAST_MUTEX InodeAllocationMutex;

	// This buffer is used for read/write operations in memory-constrained environments.
	FAST_MUTEX TempPageSizeBufferMutex;
	void* TempPageSizeBuffer;
	MDL_ONEPAGE TempPageSizeBufferMdl;

	// This buffer is used for reads of block/inode bitmaps.
	FAST_MUTEX TempBlockSizeBufferMutex;
	void* TempBlockSizeBuffer;
	PMDL TempBlockSizeBufferMdl;

	// This buffer is used for certain cleaning operations.
	MDL_ONEPAGE ZeroPageBufferMdl;

	// This is a quick reference to the root directory.
	PFCB RootInode;
};

// ** Ext2 File System object type functions **
extern POBJECT_TYPE Ext2FileSystemType;

void Ext2DeleteFileSystem(void* FileSystemV);

BSTATUS Ext2ParseFileSystem(void* FileSystemV, const char** Name, void* Context, int LoopCount, void** OutObject);

BSTATUS Ext2CreateFileSystemObject(PEXT2_FILE_SYSTEM* Out);

BSTATUS Ext2OpenInode(PEXT2_FILE_SYSTEM FileSystem, uint32_t InodeNumber, PFCB* OutFcb);

BSTATUS Ext2ReadBlockGroupDescriptor(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t BlockGroup,
	PEXT2_BLOCK_GROUP_DESCRIPTOR Descriptor
);

BSTATUS Ext2WriteBlockGroupDescriptor(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t BlockGroup,
	PEXT2_BLOCK_GROUP_DESCRIPTOR Descriptor
);

void Ext2IncrementFreeBlocksSuperBlock(
	PEXT2_FILE_SYSTEM FileSystem,
	int32_t AmountBy
);

void Ext2ModifyFreeInodesSuperBlock(
	PEXT2_FILE_SYSTEM FileSystem,
	bool Allocating
);

bool Ext2GetSuperBlockSnapshot(
	PEXT2_FILE_SYSTEM FileSystem,
	PEXT2_SUPERBLOCK OutSuperblock,
	bool ClearDirty,
	bool CopyEvenIfClean
);

BSTATUS Ext2AllocateBlocks(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t BlockHint,
	size_t BlockCount,
	uint32_t* OutBlockStart,
	size_t* OutBlockCount
);

BSTATUS Ext2FreeBlock(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t BlockIndex
);
