/***
	The Boron Operating System
	Copyright (C) 2023-2024 iProgramInCpp

Module name:
	rtl/avltree.h
	
Abstract:
	This header file provides definitions for the
	implementation of the AVL tree.
	
Author:
	iProgramInCpp - 21 November 2023
***/
#pragma once

// N.B. Pointers used here must be 4 byte aligned.  However, this isn't a problem
// because you don't see unaligned pointers often.

#include <main.h>

typedef struct AVLTREE_ENTRY_tag AVLTREE_ENTRY;
typedef struct AVLTREE_ENTRY_tag *PAVLTREE_ENTRY;
typedef uintptr_t AVLTREE_LEVEL;
typedef uintptr_t AVLTREE_KEY;

typedef bool (*PAVLTREE_TRAVERSAL_FUNCTION) (void* Context, PAVLTREE_ENTRY Entry);

struct AVLTREE_ENTRY_tag
{
	uintptr_t     ParentAndBalance;
	PAVLTREE_ENTRY Llink;
	PAVLTREE_ENTRY Rlink;
	AVLTREE_KEY    Key;
};

typedef struct AVLTREE_tag
{
	AVLTREE_ENTRY Root;
}
AVLTREE, *PAVLTREE;

#define InitializeAvlTree(t) \
	((t)->Root.ParentAndBalance = 0, (t)->Root.Llink = (t)->Root.Rlink = NULL)

#define IsEmptyAvlTree(t) \
	((t)->Root.Rlink == NULL)

#define InitializeAvlTreeEntry(t) \
	((t)->Llink = (t)->Rlink = NULL, (t)->ParentAndBalance = (t)->Key = 0)

// Note: `Item` only has its key set up. Everything
// else will be set up by the function.
// Returns the new root, and whether the entry is now in the tree.
bool InsertItemAvlTree(PAVLTREE Tree, PAVLTREE_ENTRY Item);

bool RemoveItemAvlTree(PAVLTREE Tree, PAVLTREE_ENTRY Item);

PAVLTREE_ENTRY LookUpItemAvlTree(PAVLTREE Tree, AVLTREE_KEY Key);

PAVLTREE_ENTRY LookUpItemApproximateAvlTree(PAVLTREE Tree, AVLTREE_KEY Key);

PAVLTREE_ENTRY GetFirstEntryAvlTree(PAVLTREE Tree);

PAVLTREE_ENTRY GetLastEntryAvlTree(PAVLTREE Tree);

size_t GetItemCountAvlTree(PAVLTREE Tree);

size_t GetHeightAvlTree(PAVLTREE Tree);

void TraverseInOrderAvlTree(
	PAVLTREE Tree,
	PAVLTREE_TRAVERSAL_FUNCTION Function,
	void* Context);

void TraversePreOrderAvlTree(
	PAVLTREE Tree,
	PAVLTREE_TRAVERSAL_FUNCTION Function,
	void* Context);

void TraversePostOrderAvlTree(
	PAVLTREE Tree,
	PAVLTREE_TRAVERSAL_FUNCTION Function,
	void* Context);
