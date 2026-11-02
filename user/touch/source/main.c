// 2 November 2026 - iProgramInCpp
#include <boron.h>
#include <string.h>

const char* ProgramName = "touch";

#define MAX_DIRECTORY_ENTRIES (128)

uint32_t CreateDisposition = IO_CREATE_OR_OPEN;
uint32_t CreateOptions = IO_FILE_ANY_TYPE;
bool TouchWrite = false;

BSTATUS TouchFile(const char* PathName)
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
		CreateDisposition,
		CreateOptions,
		NULL, // TargetName
		0     // TargetNameLength
	);

	if (FAILED(Status))
		return Status;

	Status = OSTouchFile(Handle, TouchWrite);

	OSClose(Handle);

	return Status;
}

void ShowHelp()
{
	OSFPrintf(
		FILE_STANDARD_ERROR,
		"Usage: %s [-chm?] [file names to touch]\n"
		"-c : Do not create the file if it doesn't exist\n"
		"-h : Modify a symbolic link rather than the file it points to\n"
		"-m : Update the modification time of the file too\n"
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
				if (ArgumentArray[i][j] == 'c') {
					CreateDisposition = IO_OPEN;
				}
				if (ArgumentArray[i][j] == 'h') {
					CreateOptions = IO_FILE_SYMBOLIC_LINK;
				}
				if (ArgumentArray[i][j] == 'm') {
					TouchWrite = true;
				}
				if (ArgumentArray[i][j] == '?') {
					ShowHelp();
				}
			}

			continue;
		}

		const char* FileName = ArgumentArray[i];
		BSTATUS Status = TouchFile(FileName);

		if (FAILED(Status))
		{
			OSFPrintf(FILE_STANDARD_ERROR, "%s: %s: %s\n", ProgramName, FileName, RtlGetStatusString(Status));
			ReturnFailure = true;
		}
	}

	return ReturnFailure ? 1 : 0;
}
