/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	mdfile.c

Abstract:
	This module implements support for ext2 metadata files.

	Metadata files are cacheable entities that are used to
	avoid an aliasing bug; if access to metadata blocks
	(like inode and block bitmaps) was to go through the
	_partition_'s page cache, it could overlap data blocks
	within the same page, which is undesirable, because on
	writeback, that data will be reverted to its initial,
	unmodified state.

	Metadata files solve this problem by moving the caching
	away from the partition object and onto the ext2 meta-
	data objects themselves.

	Note that metadata files will require a different FCB
	dispatch header and extension format, because they are
	not normal files.

Author:
	iProgramInCpp - 11 October 2026
***/
#include "ext2.h"

typedef struct
{
	PEXT2_FILE_SYSTEM FileSystem;
	int MetadataType;
	size_t SizePerBlockGroup;
}
EXT2_FCB_MD_EXT, *PEXT2_FCB_MD_EXT;

#define MDEXT(Fcb) ((PEXT2_FCB_MD_EXT)(Fcb->Extension))
#define MDFCB(Ext) CONTAINING_RECORD((Ext), FCB, Extension)

#define IOSB_STATUS(iosb, stat) (iosb->Status = stat)

#define PREP_EXT PEXT2_FCB_MD_EXT Ext = MDEXT(Fcb)

bool Ext2SeekableMetadataFile(UNUSED PFCB Fcb)
{
	return true;
}

size_t Ext2GetAlignmentInfoMetadataFile(PFCB Fcb)
{
	PREP_EXT;
	size_t Alignment = Ext->FileSystem->BlockSize;
	if (Alignment > PAGE_SIZE)
		Alignment = PAGE_SIZE;
	return Alignment;
}

BSTATUS Ext2PerformOperationMetadataFile(
	bool IsWrite,
	PIO_STATUS_BLOCK Iosb,
	PFCB Fcb,
	uint64_t Offset,
	PMDL MdlBuffer,
	UNUSED uint32_t Flags
)
{
	BSTATUS Status;
	PREP_EXT;

	// If the offset or size are not aligned to the minimum alignment, return.
	size_t ReadSize = MdlBuffer->ByteCount;
	size_t Alignment = Ext2GetAlignmentInfoMetadataFile(Fcb);
	if ((Offset & (Alignment - 1)) || (ReadSize & (Alignment - 1)))
	{
		DbgPrint(
			"Ext2%sMetadataFile: Do not support unaligned access.",
			IsWrite ? "Write" : "Read"
		);
		return IOSB_STATUS(Iosb, STATUS_UNALIGNED_OPERATION);
	}

	// Handle the case where they try to read more than 1 alignment block.
	if (MdlBuffer->ByteCount >= Alignment)
	{
		MDL_ONEPAGE SubMdl;

		size_t ReadSizeInAlignments = ReadSize / Alignment;
		for (size_t i = 0; i < ReadSizeInAlignments; i++)
		{
			MmInitializeSinglePageMdlSubset(
				&SubMdl,
				MdlBuffer,
				i * Alignment,
				Alignment,
				IsWrite ? 0 : MDL_FLAG_WRITE
			);

			Status = Ext2PerformOperationMetadataFile(
				IsWrite,
				Iosb,
				Fcb,
				Offset + i * Alignment,
				&SubMdl.Base,
				Flags
			);

			MmFreeMdl(&SubMdl.Base);

			if (SUCCEEDED(Status) || Status == STATUS_END_OF_FILE) {
				Iosb->BytesWritten = i * Alignment;

				if (Status == STATUS_END_OF_FILE) {
					MmSetIntoMdl(MdlBuffer, i * Alignment, 0, Alignment);
					Status = STATUS_SUCCESS;
				}
			}
			else {
				break;
			}
		}

		if (FAILED(Status)) {
			return IOSB_STATUS(Iosb, Status);
		}

		return Status;
	}

	// One alignment block.  Determine the offset we need to submit to the backing file.
	if (Offset >= Fcb->FileLength) {
		return IOSB_STATUS(Iosb, STATUS_END_OF_FILE);
	}

	PEXT2_SUPERBLOCK SuperBlock = &Ext->FileSystem->SuperBlock;
	uint32_t BlockGroupIndex = (uint32_t) (Offset / Ext->SizePerBlockGroup);
	uint32_t OffsetInsideGroup = (uint32_t) (Offset % Ext->SizePerBlockGroup);
	uint64_t DiskOffset;
	uint64_t BlockGroupStart = (uint64_t)(BlockGroupIndex
		* SuperBlock->BlocksPerGroup
		* Ext->FileSystem->BlockSize);
	EXT2_BLOCK_GROUP_DESCRIPTOR GroupDescriptor;

	if (Ext->MetadataType == MD_BLOCK_BITMAPS ||
		Ext->MetadataType == MD_INODE_BITMAPS ||
		Ext->MetadataType == MD_INODE_TABLES)
	{
		// Must read the block group descriptor, first.
		Status = Ext2ReadBlockGroupDescriptor(
			Ext->FileSystem,
			BlockGroupIndex,
			&GroupDescriptor
		);

		if (FAILED(Status)) {
			DbgPrint("Ext2: Failed to read block group descriptor for group %u", BlockGroupIndex);
			return IOSB_STATUS(Iosb, Status);
		}
	}

	switch (Ext->MetadataType)
	{
		case MD_SUPERBLOCK:
		{
			// The offset is hard-coded at 1024.
			DiskOffset = 1024;
			break;
		}
		case MD_SUPERBLOCK_BACKUPS:
		{
			DiskOffset = BlockGroupStart;
			break;
		}
		case MD_BLOCK_GROUP_DESCRIPTOR_BACKUPS:
		{
			if (BlockGroupIndex != 0)
			{
				DiskOffset = BlockGroupStart + Ext->FileSystem->BlockSize;
				break;
			}

			[[fallthrough]];
		}
		case MD_BLOCK_GROUP_DESCRIPTORS:
		{
			DiskOffset = Ext->FileSystem->BlockSize;
			if (DiskOffset == 1024)
				DiskOffset = 2048;
			break;
		}
		case MD_BLOCK_BITMAPS:
		{
			DiskOffset = GroupDescriptor.BlockBitmapBlockId * Ext->FileSystem->BlockSize;
			break;
		}
		case MD_INODE_BITMAPS:
		{
			DiskOffset = GroupDescriptor.InodeBitmapBlockId * Ext->FileSystem->BlockSize;
			break;
		}
		case MD_INODE_TABLES:
		{
			DiskOffset = GroupDescriptor.InodeTableBlockId * Ext->FileSystem->BlockSize;
			break;
		}

		default: {
			DbgPrint("Unimplemented metadata file type %d", Ext->MetadataType);
			ASSERT(false);
			break;
		}
	}

	DiskOffset += OffsetInsideGroup;

	return IoReadFileMdl(
		Iosb,
		Ext->FileSystem->File,
		MdlBuffer,
		Flags,
		DiskOffset,
		false // Cached
	);
}

BSTATUS Ext2ReadMetadataFile(
	PIO_STATUS_BLOCK Iosb,
	PFCB Fcb,
	uint64_t Offset,
	PMDL MdlBuffer,
	UNUSED uint32_t Flags
)
{
	return Ext2PerformOperationMetadataFile(
		false, // IsWrite
		Iosb,
		Fcb,
		Offset,
		MdlBuffer,
		Flags
	);
}

BSTATUS Ext2WriteMetadataFile(
	PIO_STATUS_BLOCK Iosb,
	PFCB Fcb,
	uint64_t Offset,
	PMDL MdlBuffer,
	UNUSED uint32_t Flags
)
{
	return Ext2PerformOperationMetadataFile(
		true, // IsWrite
		Iosb,
		Fcb,
		Offset,
		MdlBuffer,
		Flags
	);
}

void Ext2ReferenceMetadataFile(UNUSED PFCB Fcb)
{
}

void Ext2DereferenceMetadataFile(UNUSED PFCB Fcb)
{
}

IO_DISPATCH_TABLE Ext2MetadataDispatchTable =
{
	.Reference = Ext2ReferenceMetadataFile,
	.Dereference = Ext2DereferenceMetadataFile,
	.Read = Ext2ReadMetadataFile,
	.Write = Ext2WriteMetadataFile,
	.Seekable = Ext2SeekableMetadataFile,
	.GetAlignmentInfo = Ext2GetAlignmentInfoMetadataFile
};

size_t Ext2GetMetadataFileSize(PEXT2_SUPERBLOCK SuperBlock, int MetadataType, size_t* OutSizePerBlockGroup)
{
	if (MetadataType == MD_SUPERBLOCK) {
		*OutSizePerBlockGroup = 1024;
		return 1024;
	}

	size_t BlockSize = 1024 << SuperBlock->BlockSizeLog2;

	// Determine the number of block groups on this disk.
	size_t BlockGroupCount = (size_t)(
		((uint64_t)(SuperBlock->BlockCount - SuperBlock->FirstDataBlock)
			+ SuperBlock->BlocksPerGroup - 1)
			/ SuperBlock->BlocksPerGroup);

	// Determine the number of blocks an inode table requires, per block group.
	size_t InodeStructureSize = SuperBlock->Version == 0
		? EXT2_DEF_INODE_SIZE
		: SuperBlock->InodeStructureSize;
	size_t InodeTableBlocks = (SuperBlock->InodesPerGroup * InodeStructureSize + BlockSize - 1)
		/ BlockSize;
	size_t DescriptorTableBlocks =
		(BlockGroupCount * sizeof(EXT2_BLOCK_GROUP_DESCRIPTOR) + BlockSize - 1)
			/ BlockSize;

	*OutSizePerBlockGroup = BlockSize;

	switch (MetadataType)
	{
		case MD_SUPERBLOCK_BACKUPS:
		case MD_BLOCK_BITMAPS:
		case MD_INODE_BITMAPS:
			// TODO: Currently assumes the block bitmap and inode bitmap are always 1 block
			// in size at most.
			//
			// Also potentially incorrectly assumes that there are this many superblock backups.
			// There might not be. Writes to superblock backups will be dropped if the superblock
			// backup number is invalid, and reads will return just zero.
			return BlockSize * BlockGroupCount;

		case MD_INODE_TABLES:
			*OutSizePerBlockGroup = BlockSize * InodeTableBlocks;
			return BlockSize * InodeTableBlocks * BlockGroupCount;

		case MD_BLOCK_GROUP_DESCRIPTORS:
			*OutSizePerBlockGroup = BlockSize * DescriptorTableBlocks;
			return BlockSize * DescriptorTableBlocks;

		case MD_BLOCK_GROUP_DESCRIPTOR_BACKUPS:
			*OutSizePerBlockGroup = BlockSize * DescriptorTableBlocks;
			return BlockSize * DescriptorTableBlocks * BlockGroupCount;

		default:
			ASSERT(!"Unimplemented metadata type");
			return 0;
	}
}

PFILE_OBJECT Ext2CreateMetadataFile(PEXT2_FILE_SYSTEM FileSystem, int MetadataType)
{
	PFCB Fcb = IoAllocateFcb(&Ext2MetadataDispatchTable, sizeof(EXT2_FCB_MD_EXT), false);
	if (!Fcb)
		return NULL;

	PREP_EXT;
	Ext->FileSystem = FileSystem;
	Ext->MetadataType = MetadataType;

	Fcb->FileLength = Ext2GetMetadataFileSize(
		&FileSystem->SuperBlock,
		MetadataType,
		&Ext->SizePerBlockGroup
	);

	PFILE_OBJECT FileObject;
	BSTATUS Status = IoCreateFileObject(Fcb, &FileObject, 0, 0);
	if (FAILED(Status)) {
		DbgPrint(
			"Ext2CreateMetadataFile: Failed to create virtual file object: %s",
			RtlGetStatusString(Status)
		);
		IoFreeFcb(Fcb);
		return NULL;
	}

	return FileObject;
}
