/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ex/bootcfg.c
	
Abstract:
	This module parses the bootloader-provided kernel command line
	and allows for easy addressing inside of it.
	
Author:
	iProgramInCpp - 31 October 2025
***/
#include <ke.h>
#include <ex.h>
#include <string.h>

//
// The boot configuration is *fixed*, so after boot it cannot be mutated.
//
// The boot command line is one line of characters which contains
// assignments of the type KEY=value, split by spaces.
//
// Quotation marks and backslashes can be used to escape characters
// within the values assigned to keys inside the command line string.
// However, this does not apply to the keys themselves.
//
// For example, all of the following will work:
// Root=/Mount/Nvme0Disk1Part0
// Root="/Mount/Nvme0Disk1Part0"
// Root="/Mount/Nvme0Disk1Part0/My Folder"
// Root=/Mount/Nvme0Disk1Part0/My\ Folder
//
// But the following will not:
// Root = /Mount/Nvme0Disk1Part0
// "Root Folder"=/Mount/Nvme0Disk1Part0
// Root\ Folder=/Mount/Nvme0Disk1Part0
//
// Also, "yes" will be assigned to any key without an equals symbol
// to assign anything to the value.
//
// An example of a boot command line looks like this:
// Root=/Mount/Nvme0Disk1Part0 Init=/Root/Init.exe SomeFlag
//

#define MAX_KEY_LENGTH (32)
#define MAX_VALUE_LENGTH (512)

static const char* ExpFailString = "Failed to parse configuration string.";
static const char* ExpDefaultValue = CONFIG_YES;

// TODO: Implement and use a hash table and more
typedef struct
{
	LIST_ENTRY ListEntry;
	
	char* Key;
	size_t KeyLength;
	
	char* Value;
	size_t ValueLength;
}
CONFIG_ENTRY, *PCONFIG_ENTRY;

static LIST_ENTRY ExpConfigListHead =
{
	.Flink = &ExpConfigListHead,
	.Blink = &ExpConfigListHead
};

static BSTATUS ExpAssignConfigValue(const char* Key, size_t KeyLength, const char* Value)
{
	size_t ValueLength = strlen(Value);
	
	PCONFIG_ENTRY Entry = MmAllocatePool(POOL_NONPAGED, sizeof(CONFIG_ENTRY) + KeyLength + ValueLength + sizeof(uintptr_t));
	if (!Entry)
		return STATUS_INSUFFICIENT_MEMORY;
	
	Entry->Key = (char*)Entry + sizeof(*Entry);
	Entry->Value = Entry->Key + KeyLength + 1;
	
	memcpy(Entry->Key, Key, KeyLength);
	Entry->Key[KeyLength] = 0;
	
	strcpy(Entry->Value, Value);
	
	Entry->KeyLength = KeyLength;
	Entry->ValueLength = ValueLength;
	
	InsertTailList(&ExpConfigListHead, &Entry->ListEntry);
	return STATUS_SUCCESS;
}

const char* ExGetConfigValue(const char* Key, const char* ValueIfNotFound)
{
	size_t Length = strlen(Key);
	
	for (PLIST_ENTRY ListEntry = ExpConfigListHead.Flink;
		ListEntry != &ExpConfigListHead;
		ListEntry = ListEntry->Flink)
	{
		PCONFIG_ENTRY Entry = CONTAINING_RECORD(ListEntry, CONFIG_ENTRY, ListEntry);
		
		if (Entry->KeyLength != Length)
			continue;
		
		if (strcmp(Entry->Key, Key) != 0)
			continue;
		
		// Found it!
		return Entry->Value;
	}
	
	return ValueIfNotFound;
}

static void ExpCheckValueBufferSize(size_t Size)
{
	if (Size < MAX_VALUE_LENGTH)
		return;

	KeCrash(
		"%s: %s. The value may not be longer than %d bytes.",
		__func__,
		ExpFailString,
		MAX_VALUE_LENGTH
	);
}

#ifdef DEBUG
void ExDumpBootConfig()
{
	DbgPrint("Dumping boot config.  Command line: [%s].", KeLoaderParameterBlock.CommandLine);
	
	for (PLIST_ENTRY ListEntry = ExpConfigListHead.Flink;
		ListEntry != &ExpConfigListHead;
		ListEntry = ListEntry->Flink)
	{
		PCONFIG_ENTRY Entry = CONTAINING_RECORD(ListEntry, CONFIG_ENTRY, ListEntry);
		
		DbgPrint("\t%s: %s", Entry->Key, Entry->Value);
	}
}
#endif

// Initializes the boot configuration.
void ExInitBootConfig()
{
	BSTATUS Status = STATUS_SUCCESS;
	
	const char* CommandLine = KeLoaderParameterBlock.CommandLine;
	
	char* ValueBuffer = MmAllocatePool(POOL_NONPAGED, MAX_VALUE_LENGTH);
	size_t ValueBufferPlace = 0;
	if (!ValueBuffer)
		KeCrash("%s: Failed to allocate buffer for values: Out of memory", __func__);
	
	const char* KeyStart;
	const char* Pointer = CommandLine;
	while (*Pointer)
	{
		// Skip all spaces.
		while (*Pointer == ' ')
			Pointer++;
		
		if (!(*Pointer))
			break;
		
		// Key mode - scan until equals, end of string, or space.
		KeyStart = Pointer;
		while (*Pointer != '=' && *Pointer != ' ' && *Pointer)
			Pointer++;
		
		// Which one did we reach?
		if (*Pointer == ' ' || *Pointer == 0)
		{
			// End of valueless key.
			size_t KeyLength = Pointer - KeyStart;
			
			Status = ExpAssignConfigValue(KeyStart, KeyLength, ExpDefaultValue);
			if (FAILED(Status))
				KeCrash("%s: %s %s (%d)", RtlGetStatusString(Status), ExpFailString, Status);
			
			// If we have a space, skip it.
			if (*Pointer == ' ')
				Pointer++;
			
			continue;
		}
		
		// Equals, meaning the key ends here.
		ASSERT(*Pointer == '=');
		size_t KeyLength = Pointer - KeyStart;
		
		// Skip the equals, then start parsing the value.
		Pointer++;
		
		while (true)
		{
			// If this is a backslash, parse the next character unless it's also a null pointer.
			if (*Pointer == '\\')
			{
				Pointer++;
				if (*Pointer == 0)
					KeCrash("%s: %s Stray backslash in command line.", __func__, ExpFailString);
				
				goto RegularCharacter;
			}
			
			// This is not a backslash, so next check for quotation marks.
			if (*Pointer == '"' || *Pointer == '\'')
			{
				char BeginningQuoteMark = *Pointer;
				Pointer++;
				
				while (*Pointer && *Pointer != BeginningQuoteMark)
				{
					ExpCheckValueBufferSize(ValueBufferPlace);
					ValueBuffer[ValueBufferPlace++] = *Pointer;
					Pointer++;
				}
				
				// If we reached the end of the string, then the quote marks weren't terminated.
				if (*Pointer == 0)
					KeCrash("%s: %s Unterminated quotation marks.", __func__, ExpFailString);
				
				Pointer++;
				continue;
			}
			
			// Finally, check for spaces and null character, which terminate a value.
			if (*Pointer == ' ' || *Pointer == 0)
			{
				ExpCheckValueBufferSize(ValueBufferPlace);
				ValueBuffer[ValueBufferPlace] = 0;
				
				ExpAssignConfigValue(KeyStart, KeyLength, ValueBuffer);
				ValueBufferPlace = 0;
				break;
			}
			
			// Regular character, so append and continue.
		RegularCharacter:
			ExpCheckValueBufferSize(ValueBufferPlace);
			ValueBuffer[ValueBufferPlace++] = *Pointer;
			Pointer++;
		}
	}
	
#ifdef DEBUG
	ExDumpBootConfig();
#endif
}
