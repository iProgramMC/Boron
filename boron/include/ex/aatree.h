/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ex/aatree.h
	
Abstract:
	This header file provides definitions for the
	implementation of the AA tree.
	
Author:
	iProgramInCpp - 21 November 2023
***/
#pragma once

#include <main.h>

typedef struct AATREE_ENTRY_tag AATREE_ENTRY;
typedef struct AATREE_ENTRY_tag *PAATREE_ENTRY;
typedef uintptr_t AATREE_LEVEL;
typedef uintptr_t AATREE_KEY;

typedef bool (*PAATREE_TRAVERSAL_FUNCTION) (void* Context, PAATREE_ENTRY Entry);

struct AATREE_ENTRY_tag
{
	PAATREE_ENTRY Llink;
	PAATREE_ENTRY Rlink;
	AATREE_LEVEL  Level;
	AATREE_KEY    Key;
	// Value is CONTAINING_RECORD(whatever);
};

typedef struct AATREE_tag
{
	PAATREE_ENTRY Root;
}
AATREE, *PAATREE;

#define ExInitializeAaTree(t) \
	((t)->Root = NULL)

#define ExIsEmptyAaTree(t) \
	((t)->Root == NULL)

#define ExInitializeAaTreeEntry(t) \
	((t)->Llink = (t)->Rlink = NULL, (t)->Key = 0, (t)->Level = 0)

// Note: `Item` only has its key set up. Everything
// else will be set up by the function.
// Returns the new root, and whether the entry is now in the tree.
bool ExInsertItemAaTree(PAATREE Tree, PAATREE_ENTRY Item);

bool ExRemoveItemAaTree(PAATREE Tree, PAATREE_ENTRY Item);

PAATREE_ENTRY ExLookUpItemAaTree(PAATREE Tree, AATREE_KEY Key);

PAATREE_ENTRY ExGetFirstEntryAaTree(PAATREE Tree);

PAATREE_ENTRY ExGetLastEntryAaTree(PAATREE Tree);

size_t ExGetItemCountAaTree(PAATREE Tree);

size_t ExGetHeightAaTree(PAATREE Tree);

void ExTraverseInOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context);

void ExTraversePreOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context);

void ExTraversePostOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context);
