/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	dskstrct.h
	
Abstract:
	This header defines the ext2 structures as they
	are defined on disk.
	
Author:
	iProgramInCpp - 8 June 2025
***/
#pragma once

#include <main.h>

#define EXT2_SIGNATURE    (0xEF53)
#define EXT2_INVALID_NODE (~0U)

// optional features
#define EXT2_OPT_PREALLOCATE         (1 << 0)
#define EXT2_OPT_AFS_SERVER_INODES   (1 << 1)
#define EXT2_OPT_JOURNAL             (1 << 2)
#define EXT2_OPT_INODE_EXT_ATTRS     (1 << 3)
#define EXT2_OPT_FS_RESIZE_SELF      (1 << 4)
#define EXT2_OPT_DIRS_USE_HASH_INDEX (1 << 5)
#define EXT2_OPT_UNSUPPORTED_FLAGS   ((-1) ^ (0))

// read-only features
#define EXT2_ROF_SPARSE_SBLOCKS_AND_GDTS (1 << 0)
#define EXT2_ROF_FS_USES_64_BIT_SIZES    (1 << 1)
#define EXT2_ROF_DIR_IN_BINARY_TREE      (1 << 2)
#define EXT2_ROF_UNSUPPORTED_FLAGS   ((-1) ^ (EXT2_ROF_FS_USES_64_BIT_SIZES | EXT2_ROF_SPARSE_SBLOCKS_AND_GDTS))

// required features
#define EXT2_REQ_COMPRESSION_USED    (1 << 0)
#define EXT2_REQ_DIR_TYPE_FIELD      (1 << 1)
#define EXT2_REQ_REPLAY_JOURNAL      (1 << 2)
#define EXT2_REQ_USE_JOURNAL_DEV     (1 << 3)
#define EXT2_REQ_UNSUPPORTED_FLAGS   ((-1) ^ (EXT2_REQ_DIR_TYPE_FIELD))

#define EXT2_DEF_FIRST_INODE   11
#define EXT2_DEF_INODE_SIZE    128

#define BLOCK_ADDRESS(x, fs) (((uint64_t)(x)) << (fs)->BlockSizeLog2)

typedef union _EXT2_SUPERBLOCK
{
	uint8_t Bytes[1024];
	
	struct
	{
		// Base super block fields
		uint32_t InodeCount;
		uint32_t BlockCount;
		uint32_t BlockCountSuperUser;
		uint32_t FreeBlockCount;
		uint32_t FreeInodeCount;
		uint32_t FirstDataBlock;
		uint32_t BlockSizeLog2;
		uint32_t FragmentSizeLog2;
		uint32_t BlocksPerGroup;
		uint32_t FragmentsPerGroup;
		uint32_t InodesPerGroup;
		uint32_t LastMountTime;
		uint32_t LastWriteTime;
		uint16_t MountCountSinceLastCheck;
		uint16_t MaxMountCountUntilCheck;
		uint16_t Ext2Signature;
		uint16_t FileSystemState;
		uint16_t ErrorDetectedAction;
		uint16_t MinorRevLevel;
		uint32_t LastCheckTime;
		uint32_t MaximumCheckInterval;
		uint32_t CreatorOSID;
		uint32_t Version;
		uint16_t ReservedBlockUid;
		uint16_t ReservedBlockGid;
		
		// Extended super block fields (EXT2_DYNAMIC_REV)
		uint32_t FirstNonReservedInode;
		uint16_t InodeStructureSize;
		uint16_t BlockGroupNumber;
		uint32_t CompatibleFeatures;
		uint32_t IncompatibleFeatures;
		uint32_t ReadOnlyFeatures;
		uint8_t  FileSystemUUID[16];
		char     VolumeName[16];
		char     LastMountedPath[64];
		uint32_t CompressionAlgorithmBitmap;
		uint8_t  PreallocateBlockCountFile;
		uint8_t  PreallocateBlockCountDirectory;
		char     JournalUUID[16];
		uint32_t JournalInodeNumber;
		uint32_t JournalDeviceNumber;
		uint32_t LastOrphan;
		uint32_t HashSeed[4];
		uint8_t  DefaultHashVersion;
		uint8_t  Padding[3];
		uint32_t FirstMetaBlockGroupId;
	};
}
EXT2_SUPERBLOCK, *PEXT2_SUPERBLOCK;

static_assert(sizeof(EXT2_SUPERBLOCK) == 1024);

typedef union _EXT2_BLOCK_GROUP_DESCRIPTOR
{
	uint8_t Bytes[32];
	
	struct
	{
		uint32_t BlockBitmapBlockId;
		uint32_t InodeBitmapBlockId;
		uint32_t InodeTableBlockId;
		uint16_t FreeBlockCount;
		uint16_t FreeInodeCount;
		uint16_t DirectoryCount;
	};
}
EXT2_BLOCK_GROUP_DESCRIPTOR, *PEXT2_BLOCK_GROUP_DESCRIPTOR;

static_assert(sizeof(EXT2_BLOCK_GROUP_DESCRIPTOR) == 32);

typedef struct _EXT2_INODE
{
	uint16_t Mode;
	uint16_t Uid;
	uint32_t Size; // Lower 32 bits
	uint32_t AccessTime;
	uint32_t CreateTime;
	uint32_t ModifyTime;
	uint32_t DeleteTime;
	uint16_t Gid;
	uint16_t LinkCount;
	uint32_t BlockCount;
	uint32_t Flags;
	uint32_t OSSpecific1;
	
	union
	{
		struct
		{
			uint32_t DirectBlockPointer[12];
			uint32_t SinglyIndirectBlockPointer;
			uint32_t DoublyIndirectBlockPointer;
			uint32_t TriplyIndirectBlockPointer;
		};
		
		char ShortSymLinkContents[60];
	};
	
	uint32_t Generation;
	uint32_t ExtendedAttributeBlockId;
	uint32_t SizeUpper;
	uint32_t FragmentLocation;
	uint32_t OSSpecific2[3];
}
EXT2_INODE, *PEXT2_INODE;

static_assert(sizeof(EXT2_INODE) == 128);
