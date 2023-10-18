/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                           list.h                            *
\*************************************************************/
#pragma once

#include <loader.h>

typedef struct _LIST_ENTRY LIST_ENTRY, *PLIST_ENTRY;

struct _LIST_ENTRY
{
	PLIST_ENTRY Flink;
	PLIST_ENTRY Blink;
};

// void InitializeListHead(PLIST_ENTRY ListHead);
#define InitializeListHead(Head) \
	(Head)->Flink = (Head),      \
	(Head)->Blink = (Head)

// void InsertHeadList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry);
#define InsertHeadList(ListHead, Entry) \
    (Entry)->Flink = (ListHead)->Flink,   \
	(Entry)->Blink = (ListHead),          \
	(ListHead)->Flink->Blink = (Entry), \
	(ListHead)->Flink = (Entry)

// void InsertTailList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry);
#define InsertTailList(ListHead, Entry) \
    (Entry)->Blink = (ListHead)->Blink,   \
	(Entry)->Flink = (ListHead),          \
	(ListHead)->Blink->Flink = (Entry), \
	(ListHead)->Blink = (Entry)

// PLIST_ENTRY RemoveHeadList(PLIST_ENTRY ListHead);
static inline ALWAYS_INLINE UNUSED
PLIST_ENTRY RemoveHeadList(PLIST_ENTRY ListHead)
{
	PLIST_ENTRY Entry = ListHead->Flink;
	ListHead->Flink = Entry->Flink;
	ListHead->Flink->Blink = ListHead;
	return Entry;
}

// PLIST_ENTRY RemoveTailList(PLIST_ENTRY ListHead);
static inline ALWAYS_INLINE UNUSED
PLIST_ENTRY RemoveTailList(PLIST_ENTRY ListHead)
{
	PLIST_ENTRY Entry = ListHead->Blink;
	ListHead->Blink = Entry->Blink;
	ListHead->Blink->Flink = ListHead;
	return Entry;
}

// bool RemoveEntryList(PLIST_ENTRY Entry);
static inline ALWAYS_INLINE UNUSED
bool RemoveEntryList(PLIST_ENTRY Entry)
{
	PLIST_ENTRY OldFlink, OldBlink;
	OldFlink = Entry->Flink;
	OldBlink = Entry->Blink;
	OldBlink->Flink = OldFlink;
	OldFlink->Blink = OldBlink;
	return OldFlink == OldBlink;
}

// bool IsListEmpty(PLIST_ENTRY ListHead);
#define IsListEmpty(ListHead) ((ListHead)->Flink == (ListHead))
