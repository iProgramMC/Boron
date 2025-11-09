// 9 November 2025 - iProgramInCpp
#include <boron.h>
#include <string.h>
#include "config.h"

#define MAX_LINE_LENGTH 512

// Currently the configuration file allows you to specify environment variables.
//
// Each line either:
//
// - Is blank
//
// - Is a comment, starting with '#'
//
// - Is an environment variable specification of the type `[key] = [value]`.
//   Whitespace around the equals sign doesn't matter, so `KEY=VALUE`, `KEY=   VALUE`, and
//   `KEY   =   VALUE` do the same thing.

bool IsWhiteSpace(char Chr)
{
	return Chr == ' ' || Chr == '\t' || Chr == '\v' || Chr == '\r' || Chr == '\n';
}

void TrimStartWhiteSpace(char** Data)
{
	while (IsWhiteSpace(**Data))
		(*Data)++;
}

void TrimEndWhiteSpace(char* Data)
{
	size_t Length = strlen(Data);
	if (Length == 0)
		return;
	
	for (intptr_t i = Length - 1; i >= 0; i--)
	{
		if (IsWhiteSpace(Data[i]))
			Data[i] = 0;
		else
			break;
	}
}

static void DumpEnvironmentDescription(const char* Description)
{
	DbgPrint("New environment description:");
	while (*Description)
	{
		DbgPrint("\t%s", Description);
		Description += strlen(Description) + 1;
	}
}

BSTATUS InitParseConfigLine(char* Environment, size_t* EnvironmentIndex, size_t MaxEnvironmentLength, char* Data)
{
	// Trim starting whitespace.
	TrimStartWhiteSpace(&Data);
	TrimEndWhiteSpace(Data);
	
	if (*Data == 0 || *Data == '#')
		// Blank line or comment.
		return STATUS_SUCCESS;
	
	TOKENIZE_STATE TokenizeState;
	CLEAR_TOKENIZE_STATE(&TokenizeState);
	
	char* Key = OSTokenizeString(&TokenizeState, Data, "=");
	char* Value = OSTokenizeString(&TokenizeState, NULL, "");
	TrimStartWhiteSpace(&Key);
	TrimStartWhiteSpace(&Value);
	TrimEndWhiteSpace(Key);
	TrimEndWhiteSpace(Value);
	
	char* Destination = Environment + *EnvironmentIndex;
	snprintf(
		Destination,
		MaxEnvironmentLength - *EnvironmentIndex,
		"%s=%s",
		Key,
		Value
	);
	
	*EnvironmentIndex += strlen(Destination) + 1;
	return STATUS_SUCCESS;
}

BSTATUS InitParseConfig(const char* Data)
{
	BSTATUS Status;
	size_t MaxEnvironmentLength = strlen(Data) + 2;
	size_t EnvironmentIndex = 0;
	char* Environment = OSAllocate(MaxEnvironmentLength + 2);
	char* LineBuffer = OSAllocate(MAX_LINE_LENGTH);
	if (!Environment || !LineBuffer)
	{
		DbgPrint("InitParseConfig: Cannot allocate line buffer or environment description.");
		OSFree(Environment);
		OSFree(LineBuffer);
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	size_t LineBufferIndex = 0;
	int LineNumber = 0;
	while (*Data)
	{
		LineNumber++;
		while (*Data && *Data != '\n')
		{
			if (*Data == '\r')
			{
				Data++;
				continue;
			}
			
			if (LineBufferIndex + 1 >= MAX_LINE_LENGTH)
			{
				DbgPrint("InitParseConfig: Line %d is too long.", LineNumber);
				OSFree(LineBuffer);
				return STATUS_INVALID_PARAMETER;
			}
			
			LineBuffer[LineBufferIndex++] = *Data;
			Data++;
		}
		
		LineBuffer[LineBufferIndex] = 0;
		LineBufferIndex = 0;
		
		Status = InitParseConfigLine(Environment, &EnvironmentIndex, MaxEnvironmentLength, LineBuffer);
		if (FAILED(Status))
		{
			OSFree(LineBuffer);
			return Status;
		}
		
		Data++;
	}
	
	memcpy(Environment + EnvironmentIndex, "\0\0", 2);
	
	// Apply the new environment variables.
	DumpEnvironmentDescription(Environment);
	OSDLLSetEnvironmentDescription(Environment);
	return STATUS_SUCCESS;
}

BSTATUS InitLoadConfigFile(const char* File)
{
	BSTATUS Status;
	HANDLE Handle;
	OBJECT_ATTRIBUTES Attributes;
	
	Attributes.ObjectName = File;
	Attributes.ObjectNameLength = strlen(File);
	Attributes.OpenFlags = 0;
	Attributes.RootDirectory = HANDLE_NONE;
	Status = OSOpenFile(&Handle, &Attributes);
	if (FAILED(Status))
	{
		DbgPrint("Init: Failed to open config file '%s'. %s", File, RtlGetStatusString(Status));
		return Status;
	}
	
	uint64_t FileLength = 0;
	Status = OSGetLengthFile(Handle, &FileLength);
	if (FAILED(Status))
	{
		DbgPrint("Init: Failed to get size of config file '%s'. %s", File, RtlGetStatusString(Status));
		OSClose(Handle);
		return Status;
	}
	
	char* Data = OSAllocate(FileLength + 1);
	if (!Data)
	{
		DbgPrint("Init: Failed to allocate space to read config file.");
		OSClose(Handle);
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	IO_STATUS_BLOCK Iosb;
	Status = OSReadFile(&Iosb, Handle, 0, Data, FileLength, 0);
	if (FAILED(Status) || Iosb.BytesRead != FileLength)
	{
		DbgPrint("Init: Failed to read config file '%s' (read %zu bytes). %s", File, Iosb.BytesRead, RtlGetStatusString(Status));
		OSClose(Handle);
		return Status;
	}
	
	Data[FileLength] = 0;
	OSClose(Handle);
	
	return InitParseConfig(Data);
}
