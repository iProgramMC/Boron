/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	rtl/list.h
	
Abstract:
	This header file contains the definitions related
	to the linked list code.
	
	It is important to note that the list entries' memory
	is managed manually by the user of these functions.
	This is merely the implementation of the doubly circularly
	linked list data structure.
	
Author:
	iProgramInCpp - 2 October 2023
***/
#ifndef BORON_RTL_LIST_H
#define BORON_RTL_LIST_H

#include <stdbool.h>

typedef struct _LIST_ENTRY LIST_ENTRY, *PLIST_ENTRY;

struct _LIST_ENTRY
{
	PLIST_ENTRY Flink;
	PLIST_ENTRY Blink;
};

void InitializeListHead(PLIST_ENTRY ListHead);

void InsertHeadList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry);

void InsertTailList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry);

PLIST_ENTRY RemoveHeadList(PLIST_ENTRY ListHead);

bool IsListEmpty(PLIST_ENTRY ListHead);

#endif//BORON_RTL_LIST_H
