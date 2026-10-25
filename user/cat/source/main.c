// 17 October 2026 - iProgramInCpp
#include <boron.h>
#include <string.h>

const char* ProgramName = "cat";

#define MAX_WRITE_BUFFER 4096

#define MAX_DIRECTORY_ENTRIES (128)

size_t StringToInteger(const char* Ptr)
{
	size_t Value = 0;

	for (; *Ptr != 0; Ptr++)
	{
		if (*Ptr >= '0' && *Ptr <= '9') {
			Value = Value * 10 + (*Ptr - '0');
		}
	}

	return Value;
}

void PrintHelp()
{
	OSFPrintf(
		FILE_STANDARD_ERROR,
		"Usage: %s [-h?olcxatbp] [input file names, or none for stdin]\n"
		"-h, -?, --help: Show this message\n"
		"-o <filename> : Set the output file name rather than writing to standard output\n"
		"-l <limit>    : Set the maximum amount of bytes to be written to the output\n"
		"-c            : Write one character at a time, rather than the default of %d bytes\n",
		ProgramName,
		MAX_WRITE_BUFFER
	);
	OSFPrintf(
		FILE_STANDARD_ERROR,
		"-x            : Do not open an existing file, throw an error instead\n"
		"-a            : Do not truncate an existing file; append to it instead\n"
		"-t            : Truncate an existing file when opening (default)\n"
		"-b            : Buffer %d bytes between reads/writes (default)\n"
		"-p            : Report progress for each performed concatenation\n",
		MAX_WRITE_BUFFER
	);
}

void SetRootDirForObjectAttributes(POBJECT_ATTRIBUTES ObjectAttributes, const char* OrigPath)
{
	if (OrigPath[0] == '/')
		ObjectAttributes->RootDirectory = HANDLE_NONE;
	else
		ObjectAttributes->RootDirectory = OSGetCurrentDirectory();
}

bool PerformConcatenate(
	HANDLE InputFile,
	const char* Path,
	HANDLE OutputFile,
	size_t* InOutLimit,
	size_t BufferSize,
	bool ReportProgress
)
{
	DbgPrint("PerformConcatenate(%s)", Path);

	BSTATUS Status = STATUS_UNIMPLEMENTED;
	IO_STATUS_BLOCK Iosb;
	bool WasWriting;

	uint32_t ExtraFlags = 0;
	if (InputFile == OSGetStandardInput()) {
		ExtraFlags = IO_RW_FINISH_ON_NEWLINE;
	}

	uint64_t ReadSize = InOutLimit ? *InOutLimit : 0;
	uint64_t TotalWritten = 0;

	if (ReportProgress)
	{
		uint64_t FileLength = 0;

		BSTATUS Status = OSGetLengthFile(InputFile, &FileLength);
		if (FAILED(Status))
		{
			if (!ReadSize)
			{
				OSFPrintf(
					FILE_STANDARD_ERROR,
					"%s: Cannot get file length from '%s', not reporting progress: %s\n",
					Path,
					RtlGetStatusString(Status)
				);
			}

			FileLength = 0;
		}

		if (FileLength != 0 && ReadSize > FileLength)
		{
			ReadSize = FileLength;
		}
	}

	char Buffer[MAX_WRITE_BUFFER+1];

	do
	{
		size_t BytesToRead = BufferSize;
		if (InOutLimit && *InOutLimit < BufferSize) {
			BytesToRead = *InOutLimit;
		}

		if (BytesToRead == 0) {
			break;
		}

		WasWriting = false;
		Status = OSReadFile(
			&Iosb,
			InputFile,
			0, // ByteOffset
			Buffer,
			sizeof Buffer,
			IO_RW_SHARED_FILE_OFFSET | ExtraFlags
		);

		if (IOFAILED(Status)) {
			break;
		}

		if (Status == STATUS_END_OF_FILE || Iosb.BytesRead == 0)
		{
			if (ReadSize != 0 && TotalWritten != ReadSize)
			{
				// N.B. newline intentionally omitted here
				OSFPrintf(
					FILE_STANDARD_ERROR,
					"\n%s: End of file hit early, %llu bytes requested but only got %llu bytes",
					Path,
					TotalWritten,
					ReadSize
				);
			}
			break;
		}

		if (InOutLimit) {
			*InOutLimit -= Iosb.BytesRead;
		}

		Buffer[Iosb.BytesRead] = 0;

		// Write all the bytes necessary
		UNUSED uint64_t Unused = 0;
		WasWriting = true;

		size_t BytesToWrite = Iosb.BytesRead, BufferOffset = 0;
		while (BytesToWrite != 0)
		{
			DbgPrint("writing '%s'", Buffer);

			Status = OSWriteFile(
				&Iosb,
				OutputFile,
				0, // ByteOffset
				Buffer + BufferOffset,
				BytesToWrite,
				IO_RW_SHARED_FILE_OFFSET,
				&Unused
			);

			if (IOFAILED(Status)) {
				break;
			}

			TotalWritten += Iosb.BytesWritten;
			BufferOffset += Iosb.BytesWritten;
			BytesToWrite -= Iosb.BytesWritten;

			if (ReadSize != 0)
			{
				OSFPrintf(
					FILE_STANDARD_ERROR,
					"\r%s: Written %llu/%llu bytes      ",
					Path,
					TotalWritten,
					ReadSize
				);
			}
		}

		if (IOFAILED(Status)) {
			break;
		}
	}
	while (true);

	if (ReadSize != 0)
	{
		OSFPrintf(FILE_STANDARD_ERROR, "\n");
	}

	if (IOFAILED(Status))
	{
		OSFPrintf(
			FILE_STANDARD_ERROR,
			"%s: Cannot %s from '%s': %s\n",
			ProgramName,
			WasWriting ? "write" : "read",
			Path,
			RtlGetStatusString(Status)
		);
		return false;
	}

	return true;
}

int main(int ArgumentCount, char** ArgumentArray)
{
	if (ArgumentCount > 0)
		ProgramName = ArgumentArray[0];

	BSTATUS Status;
	HANDLE OutputHandle = OSGetStandardOutput();
	OBJECT_ATTRIBUTES Attributes;

	bool SpecifiedInputFile = false;
	bool ReturnFailure = false;
	bool ReportProgress = false;
	size_t ReadBufferSize = MAX_WRITE_BUFFER;
	size_t ReadLimit = 0;

	uint32_t CreateDisposition = IO_TRUNCATE_OR_CREATE;

	for (int i = 1; i < ArgumentCount; i++)
	{
		if (strcmp(ArgumentArray[i], "--help") == 0 ||
			strcmp(ArgumentArray[i], "-?") == 0 ||
			strcmp(ArgumentArray[i], "-h") == 0)
		{
			PrintHelp();
			return 0;
		}

		if (strcmp(ArgumentArray[i], "-a") == 0)
		{
			CreateDisposition = IO_CREATE_OR_OPEN;
			continue;
		}

		if (strcmp(ArgumentArray[i], "-x") == 0)
		{
			CreateDisposition = IO_CREATE;
			continue;
		}

		if (strcmp(ArgumentArray[i], "-t") == 0)
		{
			CreateDisposition = IO_TRUNCATE_OR_CREATE;
			continue;
		}

		if (strcmp(ArgumentArray[i], "-c") == 0)
		{
			ReadBufferSize = 1;
			continue;
		}

		if (strcmp(ArgumentArray[i], "-b") == 0)
		{
			ReadBufferSize = MAX_WRITE_BUFFER;
			continue;
		}

		if (strcmp(ArgumentArray[i], "-p") == 0)
		{
			ReportProgress = true;
			continue;
		}

		if (strcmp(ArgumentArray[i], "-l") == 0)
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

			i++;

			ReadLimit = StringToInteger(ArgumentArray[i]);
			continue;
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

			IO_STATUS_BLOCK Iosb;
			memset(&Iosb, 0, sizeof(Iosb));

			HANDLE NewHandle = HANDLE_NONE;
			Status = OSCreateFile(
				&NewHandle,
				&Attributes,
				&Iosb,
				0,    // FileAttributes
				0,    // AllocationSize
				CreateDisposition,
				IO_FILE_REGULAR_FILE,
				NULL,
				0
			);

			if (FAILED(Status))
			{
				OSFPrintf(
					FILE_STANDARD_ERROR,
					"%s: Cannot open '%s' for writing: %s\n",
					ProgramName,
					ArgumentArray[i],
					RtlGetStatusString(Status)
				);
				ReturnFailure = true;
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
			ReturnFailure = true;
			continue;
		}

		bool Result = PerformConcatenate(
			InputHandle,
			ArgumentArray[i],
			OutputHandle,
			ReadLimit ? &ReadLimit : NULL,
			ReadBufferSize,
			ReportProgress
		);

		if (!Result)
			ReturnFailure = true;
	}

	if (!SpecifiedInputFile)
	{
		bool Result = PerformConcatenate(
			OSGetStandardInput(),
			"Standard input",
			OutputHandle,
			ReadLimit ? &ReadLimit : NULL,
			ReadBufferSize,
			ReportProgress
		);

		if (!Result)
			ReturnFailure = true;
	}

	OSClose(OutputHandle);

	OSFPrintf(FILE_STANDARD_ERROR, "\n");

	return ReturnFailure ? 1 : 0;
}
