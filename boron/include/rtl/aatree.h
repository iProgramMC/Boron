/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	rtk/aatree.h
	
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

#define InitializeAaTree(t) \
	((t)->Root = NULL)

#define IsEmptyAaTree(t) \
	((t)->Root == NULL)

#define InitializeAaTreeEntry(t) \
	((t)->Llink = (t)->Rlink = NULL, (t)->Key = 0, (t)->Level = 0)

// Note: `Item` only has its key set up. Everything
// else will be set up by the function.
// Returns the new root, and whether the entry is now in the tree.
bool InsertItemAaTree(PAATREE Tree, PAATREE_ENTRY Item);

bool RemoveItemAaTree(PAATREE Tree, PAATREE_ENTRY Item);

PAATREE_ENTRY LookUpItemAaTree(PAATREE Tree, AATREE_KEY Key);

PAATREE_ENTRY GetFirstEntryAaTree(PAATREE Tree);

PAATREE_ENTRY GetLastEntryAaTree(PAATREE Tree);

size_t GetItemCountAaTree(PAATREE Tree);

size_t GetHeightAaTree(PAATREE Tree);

void TraverseInOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context);

void TraversePreOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context);

void TraversePostOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context);
