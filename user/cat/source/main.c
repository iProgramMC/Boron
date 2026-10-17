// 17 October 2026 - iProgramInCpp
#include <boron.h>
#include <string.h>

const char* ProgramName = "cat";

#define MAX_DIRECTORY_ENTRIES (128)

void PrintHelp()
{
	OSFPrintf(
		FILE_STANDARD_ERROR,
		"Usage: %s [-o output file name] [input file names, or none for stdin]\n",
		ProgramName
	);
}

void SetRootDirForObjectAttributes(POBJECT_ATTRIBUTES ObjectAttributes, const char* OrigPath)
{
	if (OrigPath[0] == '/')
		ObjectAttributes->RootDirectory = HANDLE_NONE;
	else
		ObjectAttributes->RootDirectory = OSGetCurrentDirectory();
}

void PerformConcatenate(HANDLE InputFile, const char* Path, HANDLE OutputFile)
{
	BSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	bool WasWriting;

	char Buffer[4096];

	do
	{
		WasWriting = false;
		Status = OSReadFile(
			&Iosb,
			InputFile,
			0, // ByteOffset
			Buffer,
			sizeof Buffer,
			IO_RW_SHARED_FILE_OFFSET
		);

		if (IOFAILED(Status)) {
			break;
		}

		if (Status == STATUS_END_OF_FILE || Iosb.BytesRead == 0) {
			break;
		}

		UNUSED uint64_t Unused = 0;
		WasWriting = true;
		Status = OSWriteFile(
			&Iosb,
			OutputFile,
			0, // ByteOffset
			Buffer,
			sizeof Buffer,
			IO_RW_SHARED_FILE_OFFSET,
			&Unused
		);

		if (IOFAILED(Status)) {
			break;
		}
	}
	while (true);

	if (IOFAILED(Status))
	{
		OSFPrintf(
			FILE_STANDARD_ERROR,
			"%s: Cannot %s from '%s': %s",
			ProgramName,
			WasWriting ? "write" : "read",
			Path,
			RtlGetStatusString(Status)
		);
	}
}

int main(int ArgumentCount, char** ArgumentArray)
{
	if (ArgumentCount > 0)
		ProgramName = ArgumentArray[0];

	BSTATUS Status;
	HANDLE OutputHandle = OSGetStandardOutput();
	OBJECT_ATTRIBUTES Attributes;

	bool SpecifiedInputFile = false;

	for (int i = 1; i < ArgumentCount; i++)
	{
		if (strcmp(ArgumentArray[i], "--help") == 0 ||
			strcmp(ArgumentArray[i], "-?") == 0)
		{
			PrintHelp();
			return 0;
		}

		if (strcmp(ArgumentArray[i], "--output") == 0 ||
			strcmp(ArgumentArray[i], "-o") == 0)
		{
			if (i + 1 >= ArgumentCount)
			{
				OSFPrintf(
					FILE_STANDARD_ERROR,
					"%s: %s requires a parameter.\n",
					ProgramName,
					ArgumentArray[i]
				);
				return 1;
			}

			OSInitializeObjectAttributes(&Attributes);
			OSSetNameObjectAttributes(&Attributes, ArgumentArray[i + 1]);
			SetRootDirForObjectAttributes(&Attributes, ArgumentArray[i + 1]);

			i++;

			HANDLE NewHandle = HANDLE_NONE;
			Status = OSOpenFile(&NewHandle, &Attributes);

			if (FAILED(Status))
			{
				OSFPrintf(
					FILE_STANDARD_ERROR,
					"%s: Cannot open '%s' for writing: %s\n",
					ProgramName,
					ArgumentArray[i],
					RtlGetStatusString(Status)
				);
				continue;
			}

			OSClose(OutputHandle);
			OutputHandle = NewHandle;
			continue;
		}

		SpecifiedInputFile = true;

		OSInitializeObjectAttributes(&Attributes);
		OSSetNameObjectAttributes(&Attributes, ArgumentArray[i]);
		SetRootDirForObjectAttributes(&Attributes, ArgumentArray[i]);

		HANDLE InputHandle = HANDLE_NONE;
		Status = OSOpenFile(&InputHandle, &Attributes);

		if (FAILED(Status))
		{
			OSFPrintf(
				FILE_STANDARD_ERROR,
				"%s: Cannot open '%s' for reading: %s\n",
				ProgramName,
				ArgumentArray[i],
				RtlGetStatusString(Status)
			);
			continue;
		}

		PerformConcatenate(InputHandle, ArgumentArray[i], OutputHandle);
	}

	if (!SpecifiedInputFile)
	{
		PerformConcatenate(OSGetStandardInput(), "Standard input", OutputHandle);
	}

	OSClose(OutputHandle);
	return 0;
}
