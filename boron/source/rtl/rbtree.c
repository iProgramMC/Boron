/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	rtl/rbtree.h
	
Abstract:
	This module implements the rank-balanced binary search tree.
	
	This implementation was adapted from FreeBSD.  It's available at:
	https://github.com/freebsd/freebsd-src/blob/main/sys/sys/tree.h
	
Author:
	iProgramInCpp - 13 August 2024
***/
#include <rtl/rbtree.h>

int RtlCompareRbTreeNodes(PRBTREE_ENTRY EntryA, PRBTREE_ENTRY EntryB)
{
	if (EntryA->Key < EntryB->Key)
		return -1;
	if (EntryA->Key > EntryB->Key)
		return 1;
	return 0;
}


// TODO: For some reason, when I compile a standard linux program, there are no warnings
// (even with -Wall -Wextra), but when I compile it for boron, there are warnings.
//
// Disable the strict aliasing warning because I'm pretty sure it'll be fine.
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#pragma GCC diagnostic push

RB_GENERATE_INTERNAL(_RBTREE_HEAD, _RBTREE_ENTRY, Entry, RtlCompareRbTreeNodes, static inline ALWAYS_INLINE);

#pragma GCC diagnostic pop

bool InsertItemRbTree(PRBTREE Tree, PRBTREE_ENTRY Item)
{
	// RB_INSERT returns NULL if the element was inserted into the tree successfully.
	//
	// It returns a pointer to the item with the colliding key if it already exists.
	return RB_INSERT(_RBTREE_HEAD, &Tree->Head, Item) == NULL;
}

bool RemoveItemRbTree(PRBTREE Tree, PRBTREE_ENTRY Item)
{
	return RB_REMOVE(_RBTREE_HEAD, &Tree->Head, Item) == Item;
}

PRBTREE_ENTRY LookUpItemRbTree(PRBTREE Tree, RBTREE_KEY Key)
{
	RBTREE_ENTRY Find;
	Find.Key = Key;
	return RB_FIND(_RBTREE_HEAD, &Tree->Head, &Find);
}

PRBTREE_ENTRY LookUpItemApproximateRbTree(PRBTREE Tree, RBTREE_KEY Key)
{
	RBTREE_ENTRY Find;
	Find.Key = Key;
	
	PRBTREE_ENTRY Found = RB_NFIND(_RBTREE_HEAD, &Tree->Head, &Find);
	if (!Found)
	{
		// No entry.  Maybe we're looking for the last one?
		Found = RB_MAX(_RBTREE_HEAD, &Tree->Head);
		if (Found->Key <= Key)
			return Found;
		
		return NULL;
	}
	
	// RB_NFIND finds the LEAST element >= the key, whereas LookUpItemApproximateRbTree
	// should find the GREATEST element <= the key.  If the keys don't match, right before
	// the found element, the element we're looking for will be located.
	//
	// Note that NULL may be returned after RB_PREV.  This is fine, we're ALWAYS looking
	// for the element whose key <= the key.
	if (Found->Key == Key)
		return Found;
	
	Found = RB_PREV(_RBTREE_HEAD, &Tree->Head, Found);
	return Found;
}

PRBTREE_ENTRY GetFirstEntryRbTree(PRBTREE Tree)
{
	return RB_MIN(_RBTREE_HEAD, &Tree->Head);
}

PRBTREE_ENTRY GetLastEntryRbTree(PRBTREE Tree)
{
	return RB_MAX(_RBTREE_HEAD, &Tree->Head);
}

size_t GetItemCountRbTree(PRBTREE Tree)
{
	size_t Count = 0;
	PRBTREE_ENTRY Entry = NULL;
	RB_FOREACH(Entry, _RBTREE_HEAD, &Tree->Head) Count++;
	return Count;
}

void TraverseRbTree(
	PRBTREE Tree,
	PRBTREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	PRBTREE_ENTRY Entry = NULL;
	RB_FOREACH(Entry, _RBTREE_HEAD, &Tree->Head)
		Function(Context, Entry);
}

PRBTREE_ENTRY GetNextEntryRbTree(PRBTREE_ENTRY Entry)
{
	return RB_NEXT(_RBTREE_HEAD, NULL, Entry);
}

PRBTREE_ENTRY GetPrevEntryRbTree(PRBTREE_ENTRY Entry)
{
	return RB_PREV(_RBTREE_HEAD, NULL, Entry);
}
