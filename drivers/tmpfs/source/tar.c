/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the main function of the
	Temporary File System driver.
	
Author:
	iProgramInCpp - 1 November 2025
***/
#include <ex.h>
#include "tmpfs.h"
#include "tar.h"

static uint32_t OctToBin(char *Data, uint32_t Size)
{
	uint32_t Value = 0;
	while (Size > 0)
	{
		Size--;
		Value *= 8;
		Value += *Data++ - '0';
	}
	return Value;
}

static const char* SanitizePath(const char* Path)
{
	const char* NewPath = Path;
	
	while (*Path)
	{
		if (*Path == '/')
			NewPath = Path + 1;
		
		Path++;
	}
	
	return NewPath;
}

static bool TrimSlashAtEnd(char* String)
{
	if (*String == 0)
		return false;
	
	String += strlen(String) - 1;
	
	if (*String == '/')
	{
		*String = 0;
		return true;
	}
	
	return false;
}

BSTATUS TmpMountTarFile(PLOADER_MODULE Module)
{
	BSTATUS Status;
	char Buffer[IO_MAX_NAME * 2];
	char SecondBuffer[IO_MAX_NAME];
	
#ifdef IS_32_BIT
	uint8_t* ModuleAddress = MmMapIoSpace(
		(uintptr_t)Module->Address - MI_IDENTMAP_START,
		Module->Size,
		MM_PTE_READWRITE,
		POOL_TAG("TarM")
	);
	
	if (!ModuleAddress)
	{
		DbgPrint("Mapping tar file failed, out of memory?");
		return STATUS_INSUFFICIENT_MEMORY;
	}
#else
	uint8_t* ModuleAddress = Module->Address;
#endif
	
	PTAR_UNIT CurrentBlock = (void*) ModuleAddress;
	PTAR_UNIT EndBlock = (void*) ModuleAddress + Module->Size;
	
	const char* SanitizedPath = SanitizePath(Module->Path);
	
	PFILE_OBJECT RootObject;
	Status = TmpCreateFile(&RootObject, NULL, 0, FILE_TYPE_DIRECTORY, NULL, NULL);
	if (FAILED(Status))
	{
		LogMsg(
			"%s: Could not create root directory for tar archive %s: %s (%d)",
			__func__,
			Module->Path,
			RtlGetStatusString(Status),
			Status
		);
		
		return Status;
	}
	
	// Unless the configuration specified an override for the name, use the module's
	// name as the mount point's name.
	snprintf(Buffer, sizeof Buffer, "NameOverride:%s", SanitizedPath);
	const char* MountName = ExGetConfigValue(Buffer, SanitizedPath);
	
	// If there is still no mount name for some reason, give it a name of our own.
	
	Status = ObLinkObject(ObGetRootDirectory(), RootObject, MountName);
	if (FAILED(Status))
	{
		LogMsg(
			"%s: Could not link root directory for tar archive %s: %s (%d)",
			__func__,
			Module->Path,
			RtlGetStatusString(Status),
			Status
		);
		
		return Status;
	}
	
	snprintf(Buffer, sizeof Buffer, "/%s", MountName);
	
	// Get the end of the root's name.  We will be constantly adding new things afterwards
	// so good to keep this around instead of re-formatting every time.
	char* RootNameEnd = Buffer + strlen(Buffer);
	size_t RemainderLength = sizeof(Buffer) - (RootNameEnd - Buffer);
	if (RootNameEnd >= Buffer + sizeof(Buffer))
	{
		KeCrash("Tmpfs: The name of the mount point, '%s', is too long.  Try a shorter one.", Buffer);
	}
	
	while (CurrentBlock < EndBlock)
	{
		const char* Path = CurrentBlock->Name;
		uint32_t FileSize = OctToBin(CurrentBlock->Size, 11);
		
		// If the path starts with "./", skip it.
		if (memcmp(Path, "./", 2) == 0)
			Path += 2;
		
		if (*Path == 0)
			goto Skip;
		
		// TODO: This relies on the tar utility to create the entries
		// for directories and place them before their files.  NanoShell's
		// tar parser doesn't have this problem, but I'm lazy right now.
		
		// First, split the directory path from the file name.
		const char* FileName = Path;
		const char* Path2 = Path;
		
		while (*Path2)
		{
			// If we have a slash, and it's not at the end.
			if (*Path2 == '/' && Path2[1] != 0)
				FileName = Path2 + 1;
			
			Path2++;
		}
		
		strncpy(RootNameEnd, "/", 2);
		strncpy(RootNameEnd + 1, Path, RemainderLength - 1);
		RootNameEnd[RemainderLength - 1] = 0;
		RootNameEnd[FileName - Path] = 0;
		TrimSlashAtEnd(Buffer);
		
		strncpy(SecondBuffer, FileName, sizeof(SecondBuffer));
		SecondBuffer[sizeof(SecondBuffer) - 1] = 0;
		
		// If it trimmed a slash, we really need a directory.
		bool ForceDirectory = TrimSlashAtEnd(SecondBuffer);
		uint8_t FileType = FILE_TYPE_UNKNOWN;
		
		switch (CurrentBlock->Type)
		{
			case 0:
			case '0':
				// Regular File
				FileType = FILE_TYPE_FILE;
				break;
			
			case '5':
				// Directory
				FileType = FILE_TYPE_DIRECTORY;
				break;
		}
		
		if (ForceDirectory)
			FileType = FILE_TYPE_DIRECTORY;
		
		if (FileType == FILE_TYPE_UNKNOWN)
		{
			DbgPrint("File %s has unknown type character '%c', skipping.", CurrentBlock->Type);
			goto Skip;
		}
		
		PFILE_OBJECT PathObject;
		Status = ObReferenceObjectByName(
			Buffer,
			HANDLE_NONE,
			0,
			IoFileType,
			(void**) &PathObject
		);
		if (FAILED(Status))
		{
			DbgPrint(
				"%s: Could not add file '%s' to directory '%s', because the directory couldn't be opened. %s (%d)",
				__func__,
				SecondBuffer,
				Buffer,
				RtlGetStatusString(Status),
				Status
			);
			goto Skip;
		}
		
		void* Data = CurrentBlock + 1;
		size_t DataSize = FileSize;
		if (FileType == FILE_TYPE_DIRECTORY)
		{
			Data = NULL;
			DataSize = 0;
		}
		
		PFILE_OBJECT TempFileObject;
		Status = TmpCreateFile(&TempFileObject, Data, DataSize, FileType, PathObject->Fcb, SecondBuffer);
		ObDereferenceObject(PathObject);
		
		if (FAILED(Status))
		{
			DbgPrint(
				"%s: Could not add file '%s' to directory '%s'. %s (%d)",
				__func__,
				SecondBuffer,
				Buffer,
				RtlGetStatusString(Status),
				Status
			);
			goto Skip;
		}
		
		ObDereferenceObject(TempFileObject);
		//DbgPrint("%s: Added %s to %s.", __func__, SecondBuffer, Buffer);
		
	Skip:
		// Skip this block, and then all of the blocks that make up
		// the file.  Each block is 512 bytes in size.
		CurrentBlock++;
		CurrentBlock += (FileSize + 511) / 512;
	}
	
	return STATUS_SUCCESS;
}
