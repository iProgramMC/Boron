/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	arch/amd64/dbg.c
	
Abstract:
	This header file contains the implementation of the
	architecture specific debugging routines.
	
Author:
	iProgramInCpp - 28 October 2023
***/
#include <ke.h>
#include <arch.h>
#include <ldr.h>
#include <hal.h>
#include <string.h>

#define KERNEL_IMAGE_BASE (0xFFFFFFFF80000000)

// Defined in misc.asm:
uintptr_t KiGetRIP();
uintptr_t KiGetRBP();

// Assume that EBP is the first thing pushed when entering a function. This is often
// the case because we specify -fno-omit-frame-pointer when compiling.
// If not, we are in trouble.
typedef struct STACK_FRAME_tag STACK_FRAME, *PSTACK_FRAME;

struct STACK_FRAME_tag
{
	PSTACK_FRAME Next;
	uintptr_t IP;
};

static void DbgResolveAddress(uintptr_t Address, char *SymbolName, size_t BufferSize)
{
	if (!Address)
	{
		// End of our stack trace
		strcpy(SymbolName, "End");
		return;
	}
	
	strcpy(SymbolName, "??");
	
	uintptr_t BaseAddress = 0;
	// Determine where that address came from.
	if (Address >= KERNEL_IMAGE_BASE)
	{
		// Easy, it's in the kernel.
		// Determine the symbol's name
		const char* Name = DbgLookUpRoutineNameByAddress(Address, &BaseAddress);
		
		if (Name)
		{
			snprintf(SymbolName,
					 BufferSize,
					 "brn!%s+%lx",
					 Name,
					 Address - BaseAddress);
		}
		return;
	}
	
	// Determine which loaded DLL includes this address.
	PLOADED_DLL Dll = NULL;
	
	for (int i = 0; i < KeLoadedDLLCount; i++)
	{
		PLOADED_DLL LoadedDll = &KeLoadedDLLs[i];
		if (LoadedDll->ImageBase <= Address && Address < LoadedDll->ImageBase + LoadedDll->ImageSize)
		{
			// It's the one!
			Dll = LoadedDll;
			break;
		}
	}
	
	if (!Dll)
		return;
	
	Address -= Dll->ImageBase;
	
	const char* Name = LdrLookUpRoutineNameByAddress(Dll, Address, &BaseAddress);
	
	if (Name)
	{
		snprintf(SymbolName,
		         BufferSize,
		         "%s!%s+%lx",
		         Dll->Name,
		         Name,
		         Address - BaseAddress);
	}
}

void DbgPrintStackTrace(uintptr_t Rbp)
{
	if (Rbp == 0)
		Rbp = KiGetRBP();
	
	PSTACK_FRAME StackFrame = (PSTACK_FRAME) Rbp;
	
	int Depth = 30;
	char Buffer[128];
	
	HalDisplayString("\tAddress         \tName\n");
	
	while (StackFrame && Depth > 0)
	{
		uintptr_t Address = StackFrame->IP;
		
		char SymbolName[64];
		DbgResolveAddress(Address, SymbolName, sizeof SymbolName);
		
		snprintf(Buffer, sizeof(Buffer), "\t%p\t%s\n", (void*) Address, SymbolName);
		HalDisplayString(Buffer);
		
		Depth--;
		StackFrame = StackFrame->Next;
	}
	
	if (Depth == 0)
		HalDisplayString("Warning, stack trace too deep, increase the depth in " __FILE__ " if you need it");
}
