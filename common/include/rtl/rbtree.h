/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	rtl/rbtree.h
	
Abstract:
	This header file provides definitions for the
	implementation of the rank-balanced tree.
	
	This implementation was adapted from FreeBSD.  It's available at:
	https://github.com/freebsd/freebsd-src/blob/main/sys/sys/tree.h
	
Author:
	iProgramInCpp - 13 August 2024
***/
#pragma once

#include <main.h>

typedef uintptr_t __uintptr_t;
typedef intptr_t __intptr_t;

// Include the FreeBSD tree header.
#include "fbsdtree.h"

typedef struct _RBTREE_ENTRY RBTREE_ENTRY, *PRBTREE_ENTRY;
typedef struct _RBTREE RBTREE, *PRBTREE;

typedef uintptr_t RBTREE_KEY;
typedef bool (*PRBTREE_TRAVERSAL_FUNCTION) (void* Context, PRBTREE_ENTRY Entry);

struct _RBTREE_HEAD;

struct _RBTREE_ENTRY
{
	RB_ENTRY(_RBTREE_ENTRY) Entry;
	uintptr_t Key;
};

struct _RBTREE
{
	RB_HEAD(_RBTREE_HEAD, _RBTREE_ENTRY) Head;
};

#define InitializeRbTree(Tree) RB_INIT(&(Tree)->Head)
#define IsEmptyRbTree(Tree)    RB_EMPTY(&(Tree)->Head)
#define InitializeRbTreeEntry(EntryP)

#ifdef __cplusplus
extern "C" {
#endif

bool InsertItemRbTree(PRBTREE Tree, PRBTREE_ENTRY Item);

bool RemoveItemRbTree(PRBTREE Tree, PRBTREE_ENTRY Item);

PRBTREE_ENTRY LookUpItemRbTree(PRBTREE Tree, RBTREE_KEY Key);

PRBTREE_ENTRY LookUpItemApproximateRbTree(PRBTREE Tree, RBTREE_KEY Key);

PRBTREE_ENTRY GetFirstEntryRbTree(PRBTREE Tree);

PRBTREE_ENTRY GetLastEntryRbTree(PRBTREE Tree);

PRBTREE_ENTRY GetRootEntryRbTree(PRBTREE Tree);

size_t GetItemCountRbTree(PRBTREE Tree);

void TraverseRbTree(
	PRBTREE Tree,
	PRBTREE_TRAVERSAL_FUNCTION Function,
	void* Context);
	
PRBTREE_ENTRY GetNextEntryRbTree(PRBTREE_ENTRY);
PRBTREE_ENTRY GetPrevEntryRbTree(PRBTREE_ENTRY);

#ifdef __cplusplus
}
#endif
