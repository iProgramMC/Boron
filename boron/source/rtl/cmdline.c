/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	rtl/cmdline.c
	
Abstract:
	This module implements the command line parser, as well as an environment
	variable related utility function.
	
Author:
	iProgramInCpp - 8 November 2025
***/
#ifdef KERNEL

#include <mm.h>
#include <string.h>

#define RtlAllocate(size) MmAllocatePool(POOL_PAGED, size)
#define RtlFree(ptr) MmFreePool(ptr)

#else

#include <boron.h>
#include <string.h>
#include <rtl/assert.h>
#define RtlAllocate(size) OSAllocate(size)
#define RtlFree(ptr) OSFree(ptr)

#endif

// Unlike strlen(), this counts the amount of characters in an environment
// variable description.  Environment variables are separated with "\0" and
// the final environment variable finishes with two "\0" characters.
//
// NOTE: This is also used for the command line arguments.
size_t RtlEnvironmentLength(const char* Description)
{
	size_t Length = 2;
	while (Description[0] != '\0' || Description[1] != '\0') {
		Description++;
		Length++;
	}
	
	return Length;
}

// Parses a command line string into a command line description.
//
// A command line description is a null-character-separated list of command
// line arguments. A command line string is a space-separated list of arguments
// that can include features like backslashes and quote marks, which will need
// to be treated separately.
//
// Note that shells such as bash DO NOT use this function. This function is only
// meant for internal OS programs such as the full screen terminal.
//
// Also note that special escapes such as '\n' are not supported, and they will
// just escape the character instead, in this case, '\n' becomes 'n'.
//
// Can return:
// - STATUS_INSUFFICIENT_MEMORY if not enough memory was available to allocate
//   the output description.
// - STATUS_INVALID_PARAMETER if there was a problem parsing the command line
//   string.
BSTATUS RtlCommandLineStringToDescription(const char* String, char** OutDescription)
{
#define ASSERT_IN_BOUNDS ASSERT(Index < StringLength + 2)
	
	size_t StringLength = strlen(String);
	size_t Index = 0;
	
	// The command line description will only be as long as its components,
	// however, there will need to be two extra zeroes at the end.
	char* Description = RtlAllocate(StringLength + 2);
	if (!Description)
		return STATUS_INSUFFICIENT_MEMORY;
	
	while (*String)
	{
		if (*String == ' ')
		{
			// Space, so need to terminate the current parameter.
			Description[Index++] = '\0';
			ASSERT_IN_BOUNDS;
			String++;
			continue;
		}
		
		if (*String == '\\')
		{
			// Backslash, so skip the backslash and use the next character unchanged.
			String++;
			if (*String == 0)
			{
				DbgPrint("RtlCommandLineStringToDescription: Stray backslash in command line.");
				RtlFree(Description);
				return STATUS_INVALID_PARAMETER;
			}
			
			goto NormalCharacter;
		}
		
		if (*String == '"' || *String == '\'')
		{
			// Quotes.
			char BeginningQuoteMark = *String;
			String++;
			
			while (*String && *String != BeginningQuoteMark)
			{
				Description[Index++] = *String;
				ASSERT_IN_BOUNDS;
				String++;
			}
			
			if (*String == 0)
			{
				DbgPrint("RtlCommandLineStringToDescription: Unterminated quotation marks.");
				RtlFree(Description);
				return STATUS_INVALID_PARAMETER;
			}
			
			String++;
			continue;
		}
		
	NormalCharacter:
		Description[Index++] = *String;
		ASSERT_IN_BOUNDS;
		String++;
	}
	
	// Terminate with two null characters.
	Description[Index++] = 0;
	ASSERT_IN_BOUNDS;
	Description[Index] = 0;
	ASSERT_IN_BOUNDS;
	
	*OutDescription = Description;
	return STATUS_SUCCESS;
}

