// 2 November 2026 - iProgramInCpp
#include <boron.h>
#include <string.h>

const char* ProgramName = "sync";

#define MAX_DIRECTORY_ENTRIES (128)

BSTATUS SyncFile(const char* PathName)
{
	HANDLE Handle = HANDLE_NONE;
	HANDLE CurrentDirectory = OSGetCurrentDirectory();
	OBJECT_ATTRIBUTES Attributes;
	OSInitializeObjectAttributes(&Attributes);
	OSSetNameObjectAttributes(&Attributes, PathName);
	Attributes.RootDirectory = CurrentDirectory;

	IO_STATUS_BLOCK Iosb;
	BSTATUS Status = OSCreateFile(
		&Handle,
		&Attributes,
		&Iosb,
		0, // FileAttributes
		0, // AllocationSize
		IO_OPEN,
		IO_FILE_ANY_TYPE,
		NULL, // TargetName
		0     // TargetNameLength
	);

	if (FAILED(Status))
		return Status;

	Status = OSFlushFileCache(Handle);

	OSClose(Handle);

	return Status;
}

void ShowHelp()
{
	OSFPrintf(
		FILE_STANDARD_ERROR,
		"Usage: %s [-?] [file names to sync to disk]\n"
		"-? : Show this message\n",
		ProgramName
	);
}

int main(int ArgumentCount, char** ArgumentArray)
{
	if (ArgumentCount > 0)
		ProgramName = ArgumentArray[0];

	if (ArgumentCount <= 1)
	{
		ShowHelp();
		return 1;
	}

	bool ReturnFailure = false;

	for (int i = 1; i < ArgumentCount; i++)
	{
		if (ArgumentArray[i][0] == '-')
		{
			for (int j = 1; ArgumentArray[i][j] != 0; j++)
			{
				if (ArgumentArray[i][j] == '?') {
					ShowHelp();
				}
			}

			continue;
		}

		const char* FileName = ArgumentArray[i];
		BSTATUS Status = SyncFile(FileName);

		if (FAILED(Status))
		{
			OSFPrintf(FILE_STANDARD_ERROR, "%s: %s: %s\n", ProgramName, FileName, RtlGetStatusString(Status));
			ReturnFailure = true;
		}
	}

	return ReturnFailure ? 1 : 0;
}
