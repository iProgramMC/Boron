// 31 March 2026 - iProgramInCpp
#include <boron.h>
#include <string.h>

const char* ProgramName = "ls";

#define MAX_DIRECTORY_ENTRIES (128)

void PrintListing(HANDLE DirectoryHandle, const char* DirectoryName)
{
	PIO_DIRECTORY_ENTRY Entries = OSAllocate(sizeof(IO_DIRECTORY_ENTRY) * MAX_DIRECTORY_ENTRIES);
	
	for (size_t i = 0; ; i += MAX_DIRECTORY_ENTRIES)
	{
		IO_STATUS_BLOCK Iosb;
		BSTATUS Status = OSReadDirectoryEntries(&Iosb, DirectoryHandle, MAX_DIRECTORY_ENTRIES, Entries);
		if (IOFAILED(Status))
		{
			OSFPrintf(
				FILE_STANDARD_ERROR,
				"%s: Could not read from '%s': %s\n",
				ProgramName,
				DirectoryName,
				RtlGetStatusString(Status)
			);
			return;
		}
		
		for (size_t j = 0; j < Iosb.EntriesRead; j++)
		{
			PIO_DIRECTORY_ENTRY Entry = &Entries[j];
			
			OSPrintf("%s\n", Entry->Name);
		}
		
		if (Status == STATUS_END_OF_FILE || Iosb.EntriesRead == 0)
			break;
	}
}

int _start(int ArgumentCount, char** ArgumentArray)
{
	HANDLE CurrentDirectory = OSGetCurrentDirectory();

	if (ArgumentCount > 0)
		ProgramName = ArgumentArray[0];
	
	// check for options
	HANDLE* DirectoriesToList = OSAllocate(sizeof(HANDLE*) * ArgumentCount);
	const char** DirectoryNamesToList = OSAllocate(sizeof(const char**) * ArgumentCount);
	int DirectoriesToListCount = 0;
	
	if (!DirectoriesToList || !DirectoryNamesToList)
	{
		OSFPrintf(FILE_STANDARD_ERROR, "%s: Out of memory.\n", ProgramName);
		return 0;
	}
	
	for (int i = 1; i < ArgumentCount; i++)
	{
		if (ArgumentArray[i][0] == '-')
		{
			// decode options here
			continue;
		}
		
		HANDLE Directory;
		OBJECT_ATTRIBUTES Attributes;
		OSInitializeObjectAttributes(&Attributes);
		OSSetNameObjectAttributes(&Attributes, ArgumentArray[i]);
		Attributes.RootDirectory = CurrentDirectory;
		
		BSTATUS Status = OSOpenFile(&Directory, &Attributes);
		if (FAILED(Status))
		{
			OSFPrintf(
				FILE_STANDARD_ERROR,
				"%s: Could not open '%s': %s\n",
				ProgramName,
				ArgumentArray[i],
				RtlGetStatusString(Status)
			);
			continue;
		}
		
		// Opened.
		DirectoriesToList[DirectoriesToListCount] = Directory;
		DirectoryNamesToList[DirectoriesToListCount] = ArgumentArray[i];
		DirectoriesToListCount++;
	}
	
	if (DirectoriesToListCount == 0)
	{
		DirectoriesToList[DirectoriesToListCount] = CurrentDirectory;
		DirectoryNamesToList[DirectoriesToListCount] = "Current Directory";
		DirectoriesToListCount++;
	}
	
	for (int i = 0; i < DirectoriesToListCount; i++)
	{
		if (DirectoriesToListCount != 1)
			OSPrintf("Directory of '%s':\n", DirectoryNamesToList[i]);
		
		PrintListing(DirectoriesToList[i], DirectoryNamesToList[i]);
	}
	
	for (int i = 0; i < DirectoriesToListCount; i++)
	{
		if (DirectoriesToList[i] != CurrentDirectory)
			OSClose(DirectoriesToList[i]);
	}
	
	OSFree(DirectoriesToList);
	OSFree(DirectoryNamesToList);
	return 0;
}
