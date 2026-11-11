/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	dir.c

Abstract:
	This module manages Ext2 directory parsing and lookup logic.

Author:
	iProgramInCpp - 11 November 2026
***/
#include "ext2.h"
#include "inode.h"

BSTATUS Ext2ReadDir(PIO_STATUS_BLOCK Iosb, PFILE_OBJECT FileObject, uint64_t Offset, uint64_t Version, PIO_DIRECTORY_ENTRY DirectoryEntry)
{
	BSTATUS Status;
	PFCB Fcb = FileObject->Fcb;
	PREP_EXT;
	PEXT2_FILE_SYSTEM FileSystem = Ext->OwnerFS;

	// TODO: Check if this file was modified by comparing the Version
	// with the FCB's version.  If it was, then we need to re-validate
	// the current offset.
	//
	// Linux reads directory entries until the current offset is overtaken, probably
	// to avoid looping situations.  We should do the same.
	(void) Version;

	EXT2_DIRENT Dirent;
	uint64_t NewVersion = Ext->Version;

	if (Offset >= Ext2FileSize(Fcb))
		return IOSB_STATUS(Iosb, STATUS_END_OF_FILE);

	uint32_t ReadFlags = IO_RW_BYPASS_TYPE_CHECK;

	// Since we can only do reads at the moment, just read.
	Status = IoReadFile(Iosb, FileObject, &Dirent, sizeof(EXT2_DIRENT), ReadFlags, Offset, true, true);
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);

	if (Iosb->BytesRead < sizeof(EXT2_DIRENT))
		return IOSB_STATUS(Iosb, STATUS_END_OF_FILE);

	// Read the name, and ensure fits within our IO_NAME_MAX-1 characters.
	size_t NameLength = Dirent.NameLength;
	if (FileSystem->SuperBlock.Version == 0)
		NameLength = Dirent.NameLength16;

	if (NameLength > IO_MAX_NAME - 1)
		NameLength = IO_MAX_NAME - 1;

	Status = IoReadFile(Iosb, FileObject, &DirectoryEntry->Name, NameLength, ReadFlags, Offset + sizeof(EXT2_DIRENT), true, true);
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);

	if (Iosb->BytesRead < NameLength)
		return IOSB_STATUS(Iosb, STATUS_END_OF_FILE);

	DirectoryEntry->Name[NameLength] = 0;

	// Also fill in the inode # and type.
	DirectoryEntry->InodeNumber = Dirent.InodeNumber;

	if (FileSystem->SuperBlock.Version == 0)
		DirectoryEntry->Type = FILE_TYPE_UNKNOWN;
	else
		DirectoryEntry->Type = Ext2TypeIndicatorToFileType(Dirent.TypeIndicator);

	Iosb->ReadDir.NextOffset = Offset + Dirent.RecordLength;
	Iosb->ReadDir.Version = NewVersion;

	return IOSB_STATUS(Iosb, STATUS_SUCCESS);
}

BSTATUS Ext2ParseDir(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	const char* ParsePath,
	UNUSED int ParseLoopCount,
	UNUSED PIO_PARSE_CONTEXT ParseContext
)
{
	PFCB InitialFcb = FileObject->Fcb;

	if (InitialFcb->FileType == FILE_TYPE_DIRECTORY)
	{
		if (*ParsePath == '\0' || *ParsePath == OB_PATH_SEPARATOR)
			goto ParsedThis;

		// This is a directory, so let's read it until we find something.
		// Note: The FCB is locked shared now.
		IO_STATUS_BLOCK Iosb2;
		IO_DIRECTORY_ENTRY DirEnt;
		BSTATUS Status;
		uint64_t Offset = 0, Version = 0;

		while (true)
		{
			Status = Ext2ReadDir(&Iosb2, FileObject, Offset, Version, &DirEnt);
			if (Status == STATUS_END_OF_FILE)
				break;

			if (FAILED(Status))
				return IOSB_STATUS(Iosb, Status);

			Offset  = Iosb2.ReadDir.NextOffset;
			Version = Iosb2.ReadDir.Version;

			size_t Match = ObMatchPathName(DirEnt.Name, ParsePath);
			if (!Match)
				continue;

			// Path name matched! Load the inode.
			PEXT2_FILE_SYSTEM FileSystem = EXT(InitialFcb)->OwnerFS;
			Status = Ext2OpenInode(FileSystem, DirEnt.InodeNumber, &Iosb->ParseDir.FoundFcb);
			if (FAILED(Status))
				return IOSB_STATUS(Iosb, Status);

			Iosb->ParseDir.ReparsePath = ParsePath + Match;
			return IOSB_STATUS(Iosb, STATUS_SUCCESS);
		}

		// Not Found
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
			Ext2ReferenceInode(InitialFcb);
			return IOSB_STATUS(Iosb, STATUS_SUCCESS);
		}

		return IOSB_STATUS(Iosb, STATUS_NOT_A_DIRECTORY);
	}
}
