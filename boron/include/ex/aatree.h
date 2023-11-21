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

typedef void (*PAATREE_TRAVERSAL_FUNCTION) (void* Context, PAATREE_ENTRY Entry);

struct AATREE_ENTRY_tag
{
	PAATREE_ENTRY Llink;
	PAATREE_ENTRY Rlink;
	AATREE_LEVEL  Level;
	AATREE_KEY    Key;
	// Value is CONTAINING_RECORD(whatever);
};

// Note: `Item` only has its key set up. Everything
// else will be set up by the function.
// Returns the new root, and whether the entry is now in the tree.
PAATREE_ENTRY ExInsertAaTree(PAATREE_ENTRY Root, PAATREE_ENTRY Item, bool* Inserted);

PAATREE_ENTRY ExDeleteAaTree(PAATREE_ENTRY Root, PAATREE_ENTRY Item);

PAATREE_ENTRY ExLookUpAaTree(PAATREE_ENTRY Root, AATREE_KEY Key);

PAATREE_ENTRY ExGetFirstEntryAaTree(PAATREE_ENTRY Root);

PAATREE_ENTRY ExGetLastEntryAaTree(PAATREE_ENTRY Root);

size_t ExGetSizeAaTree(PAATREE_ENTRY Root);

void ExTraverseInOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context);

void ExTraversePreOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context);

void ExTraversePostOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context);
