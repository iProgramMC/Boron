#include <boron.h>
#include <string.h>
#include "pebteb.h"
#include <_stb_sprintf.h>

typedef struct
{
	size_t BytesPrinted;
	int FileIndex;
}
PRINTF_CALLBACK_STRUCT, *PPRINTF_CALLBACK_STRUCT;

static char* OSPrintfCallback(const char* Buffer, void* User, int Length)
{
	PPRINTF_CALLBACK_STRUCT Pfcs = User;
	
	IO_STATUS_BLOCK Iosb;
	BSTATUS Status = OSWriteFile(
		&Iosb,
		OSDLLGetCurrentPeb()->StandardIO[Pfcs->FileIndex],
		0,
		Buffer,
		Length,
		IO_RW_APPEND,
		NULL
	);
	
	if (IOSUCCEEDED(Status))
		Pfcs->BytesPrinted += Iosb.BytesWritten;
	
	return (char*) Buffer;
}

size_t OSFPrintf(int FileIndex, const char* Format, ...)
{
	char Buffer[STB_SPRINTF_MIN];
	PRINTF_CALLBACK_STRUCT Pfcs;
	
	if (FileIndex < 0 || FileIndex > FILE_STANDARD_ERROR)
		return 0;
	
	Pfcs.FileIndex = FileIndex;
	Pfcs.BytesPrinted = 0;
	
	va_list VarArgs;
	va_start(VarArgs, Format);
	vsprintfcb(OSPrintfCallback, &Pfcs, Buffer, Format, VarArgs);
	va_end(VarArgs);
	
	return Pfcs.BytesPrinted;
}

size_t OSPrintf(const char* Format, ...)
{
	char Buffer[STB_SPRINTF_MIN];
	PRINTF_CALLBACK_STRUCT Pfcs;
	
	Pfcs.FileIndex = FILE_STANDARD_INPUT;
	Pfcs.BytesPrinted = 0;
	
	va_list VarArgs;
	va_start(VarArgs, Format);
	vsprintfcb(OSPrintfCallback, &Pfcs, Buffer, Format, VarArgs);
	va_end(VarArgs);
	
	return Pfcs.BytesPrinted;
	
}
