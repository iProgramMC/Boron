//
//  onexit.c
//  Copyright (C) 2026 iProgramInCpp
//  Created on 9/4/2026.
//
//  Implements the OSRegisterExitCallback() function, as well as the logic to call destructors upon
//  an app's exit.
//
#include <boron.h>
#include <rtl/assert.h>
#include "dll.h"

typedef struct
{
	LIST_ENTRY Entry;
	PEXIT_CALLBACK Callback;
	void* Context;
	void* UniqueIdentifier;
}
EXIT_CALLBACK_ITEM, *PEXIT_CALLBACK_ITEM;

static OS_CRITICAL_SECTION ExitCallbackCS;
static LIST_ENTRY ExitCallbackList;

HIDDEN
void OSInitializeExitCallbackList()
{
	InitializeListHead(&ExitCallbackList);
	if (FAILED(OSInitializeCriticalSection(&ExitCallbackCS))) {
		DbgPrint("OSInitializeCriticalSection(&ExitCallbackCS) failed!");
		RtlAbort();
	}
}

BSTATUS OSRegisterExitCallback(
	PEXIT_CALLBACK Callback,
	void* Context,
	void* UniqueIdentifier
)
{
	OSEnterCriticalSection(&ExitCallbackCS);
	
	PEXIT_CALLBACK_ITEM Item = OSAllocate(sizeof(EXIT_CALLBACK_ITEM));
	if (!Item) {
		OSLeaveCriticalSection(&ExitCallbackCS);
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	Item->Callback = Callback;
	Item->Context = Context;
	Item->UniqueIdentifier = UniqueIdentifier;
	
	InsertTailList(&ExitCallbackList, &Item->Entry);
	
	OSLeaveCriticalSection(&ExitCallbackCS);
	return STATUS_SUCCESS;
}

void OSCallExitCallbacks(void* UniqueIdentifier)
{
	OSEnterCriticalSection(&ExitCallbackCS);
	
	for (PLIST_ENTRY ListEntry = ExitCallbackList.Flink;
		ListEntry != &ExitCallbackList;
		ListEntry = ListEntry->Flink)
	{
		PEXIT_CALLBACK_ITEM Item = CONTAINING_RECORD(ListEntry, EXIT_CALLBACK_ITEM, Entry);
		
		if (!Item->Callback)
			continue;
		
		if (Item->UniqueIdentifier == UniqueIdentifier)
		{
			Item->Callback(Item->Context);
			Item->Callback = NULL;
		}
	}
	
	OSLeaveCriticalSection(&ExitCallbackCS);
}

NO_RETURN
void OSExitProcess(int ExitCode)
{
	OSCallExitCallbacks(NULL);
	OSDLLDestroySharedObjects();
	OSExitProcessInternal(ExitCode);
}
