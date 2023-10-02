/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	rtl/list.c
	
Abstract:
	This module implements the linked list functionality.
	
Author:
	iProgramInCpp - 2 October 2023
***/
#include <rtl/list.h>

void InitializeListHead(PLIST_ENTRY ListHead)
{
	ListHead->Flink = ListHead;
	ListHead->Blink = ListHead;
}

void InsertHeadList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
{
	Entry->Flink =  ListHead->Flink;
	Entry->Blink = ListHead;
	ListHead->Flink->Blink = Entry;
	ListHead->Flink = Entry;
}

void InsertTailList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
{
	Entry->Blink =  ListHead->Blink;
	Entry->Flink = ListHead;
	ListHead->Blink->Flink = Entry;
	ListHead->Blink = Entry;
}

PLIST_ENTRY RemoveHeadList(PLIST_ENTRY ListHead)
{
	PLIST_ENTRY Entry = ListHead->Flink;
	ListHead->Flink = Entry->Flink;
	ListHead->Flink->Blink = ListHead;
	return Entry;
}

PLIST_ENTRY RemoveTailList(PLIST_ENTRY ListHead)
{
	PLIST_ENTRY Entry = ListHead->Blink;
	ListHead->Blink = Entry->Blink;
	ListHead->Blink->Flink = ListHead;
	return Entry;
}

bool RemoveEntryList(PLIST_ENTRY Entry)
{
	PLIST_ENTRY OldFlink, OldBlink;
	OldFlink = Entry->Flink;
	OldBlink = Entry->Blink;
	OldBlink->Flink = OldFlink;
	OldFlink->Blink = OldBlink;
	return OldFlink == OldBlink;
}

bool IsListEmpty(PLIST_ENTRY ListHead)
{
	return ListHead->Flink == ListHead;
}
