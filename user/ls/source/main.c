// 31 March 2026 - iProgramInCpp
#include <boron.h>
#include <string.h>

const char* ProgramName = "ls";

#define MAX_DIRECTORY_ENTRIES (128)

bool ListLong = false;
bool ShowFileIDs = false;
bool ShowDotFiles = false;
bool ShowLengths = true;

void ShowListedFile(HANDLE DirectoryHandle, PIO_DIRECTORY_ENTRY DirectoryEntry)
{
	if (!ShowDotFiles && DirectoryEntry->Name[0] == '.') {
		return;
	}

	if (!ListLong) {
		OSPrintf("%s\n", DirectoryEntry->Name);
		return;
	}

	// Long format
	char BufferMemory[IO_MAX_NAME + 60];
	char* Buffer = BufferMemory;
	size_t BufferSize = sizeof(BufferMemory);
	size_t Chars;

	if (ShowFileIDs)
	{
		snprintf(Buffer, BufferSize, "%7llu ", DirectoryEntry->InodeNumber);

		Chars = strlen(Buffer);
		Buffer += Chars;
		BufferSize -= Chars;
	}

	const char* TypeDescriptor;
	switch (DirectoryEntry->Type)
	{
		default:
		case FILE_TYPE_UNKNOWN: TypeDescriptor = " (unknown)"; break;
		case FILE_TYPE_DIRECTORY: TypeDescriptor = "/"; break;
		case FILE_TYPE_FILE: TypeDescriptor = ""; break;
		case FILE_TYPE_BLOCK_DEVICE: TypeDescriptor = " (blk)"; break;
		case FILE_TYPE_CHARACTER_DEVICE: TypeDescriptor = " (chr)"; break;
		case FILE_TYPE_PIPE: TypeDescriptor = " (fifo)"; break;
		case FILE_TYPE_SOCKET: TypeDescriptor = " (socket)"; break;
		case FILE_TYPE_SYMBOLIC_LINK: TypeDescriptor = "@"; break;
	}

	if (ShowLengths)
	{
		uint64_t FileLength = 0;

		HANDLE FileHandle;
		OBJECT_ATTRIBUTES Attributes;
		OSInitializeObjectAttributes(&Attributes);
		OSSetNameObjectAttributes(&Attributes, DirectoryEntry->Name);
		Attributes.RootDirectory = DirectoryHandle;

		BSTATUS Status = OSOpenFile(&FileHandle, &Attributes);
		if (FAILED(Status))
		{
			OSFPrintf(
				FILE_STANDARD_ERROR,
				"%s: cannot open '%s': %s",
				ProgramName,
				DirectoryEntry->Name,
				RtlGetStatusString(Status)
			);
			return;
		}

		Status = OSGetLengthFile(FileHandle, &FileLength);
		if (FAILED(Status))
		{
			OSFPrintf(
				FILE_STANDARD_ERROR,
				"%s: cannot get length of '%s': %s",
				ProgramName,
				DirectoryEntry->Name,
				RtlGetStatusString(Status)
			);
			return;
		}

		snprintf(Buffer, BufferSize, "%7llu ", FileLength);
		Chars = strlen(Buffer);
		Buffer += Chars;
		BufferSize -= Chars;

		OSClose(FileHandle);
	}

	snprintf(Buffer, BufferSize, "%s%s", DirectoryEntry->Name, TypeDescriptor);
	Chars = strlen(Buffer);
	Buffer += Chars;
	BufferSize -= Chars;

	OSPrintf("%s\n", BufferMemory);
}

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

			// TODO: Add a feature to open by ID/inode number
			ShowListedFile(DirectoryHandle, Entry);
		}

		if (Status == STATUS_END_OF_FILE || Iosb.EntriesRead == 0)
			break;
	}
}

void ShowHelp()
{
	OSFPrintf(
		FILE_STANDARD_ERROR,
		"Usage: %s [-alzi] [directories to list]\n"
		"-a : Show files prefixed with a period\n"
		"-l : Show long listing format\n"
		"-z : Do not show file lengths\n"
		"-i : Show file IDs in long listing format\n",
		ProgramName
	);
}

void DecodeOptions(const char* Options)
{
	for (; *Options != 0; Options++)
	{
		switch (*Options)
		{
			case 'l':
				ListLong = true;
				break;

			case 'i':
				ShowFileIDs = true;
				break;

			case 'z':
				ShowLengths = false;
				break;

			case 'a':
				ShowDotFiles = true;
				break;

			default:
				ShowHelp();
				OSExitProcess(1);
		}
	}
}

int main(int ArgumentCount, char** ArgumentArray)
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
			DecodeOptions(ArgumentArray[i] + 1);
			continue;
		}

		HANDLE Directory;
		OBJECT_ATTRIBUTES Attributes;
		OSInitializeObjectAttributes(&Attributes);
		OSSetNameObjectAttributes(&Attributes, ArgumentArray[i]);

		if (ArgumentArray[i][0] == '/')
			Attributes.RootDirectory = HANDLE_NONE;
		else
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
