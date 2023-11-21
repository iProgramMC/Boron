/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ex/aatree.h
	
Abstract:
	This module implements the AA binary search tree.
	
Author:
	iProgramInCpp - 21 November 2023
***/
#include <ex/aatree.h>

static PAATREE_ENTRY EiSkewAaTree(PAATREE_ENTRY Root)
{
	if (!Root)
		return NULL;

	if (Root->Llink == NULL)
		return Root;

	if (Root->Llink->Level == Root->Level)
	{
		// Swap the pointers of horizontal left links.
		PAATREE_ENTRY Temp = Root->Llink;
		Root->Llink = Temp->Rlink;
		Temp->Rlink = Root;
		return Temp;
	}

	return Root;
}

static PAATREE_ENTRY EiSplitAaTree(PAATREE_ENTRY Root)
{
	if (Root == NULL)
		return NULL;

	if (Root->Rlink == NULL || Root->Rlink->Rlink == NULL)
		// No need to split
		return Root;

	if (Root->Level != Root->Rlink->Rlink->Level)
		// No need to split
		return Root;

	// We have two horizontal right links. Take the middle node,
	// elevate it, and then return it.

	PAATREE_ENTRY Temp = Root->Rlink;

	Root->Rlink = Temp->Llink;
	Temp->Llink = Root;
	Temp->Level++;

	return Temp;
}

static PAATREE_ENTRY EiInsertItemAaTree(PAATREE_ENTRY Root, PAATREE_ENTRY Item, bool* Inserted)
{
	// Optimistically assume the item will be inserted.
	*Inserted = true;

	if (!Root)
	{
		// Initialize a new leaf node.
		Item->Llink = Item->Rlink = NULL;
		Item->Level = 1;
		return Item;
	}
	else if (Item->Key < Root->Key)
	{
		// Perform insertion recursively in the Llink entry.
		Root->Llink = EiInsertItemAaTree(Root->Llink, Item, Inserted);
	}
	else if (Item->Key > Root->Key)
	{
		// Perform insertion recursively in the Llink entry.
		Root->Rlink = EiInsertItemAaTree(Root->Rlink, Item, Inserted);
	}
	else
	{
		// Item with the same key already exists.
		*Inserted = false;
	}

	// Perform a skew and split. If not inserted, don't bother.
	if (*Inserted)
	{
		Root = EiSkewAaTree(Root);
		Root = EiSplitAaTree(Root);
	}

	// Return the new root.
	return Root;
}

static bool EiIsLeafAaTree(PAATREE_ENTRY Item)
{
	return Item->Llink == NULL && Item->Rlink == NULL;
}

static AATREE_LEVEL MinLevel(
	AATREE_LEVEL a,
	AATREE_LEVEL b)
{
	return a < b ? a : b;
}

static PAATREE_ENTRY EiDecreaseLevelAaTree(PAATREE_ENTRY Root)
{
	if (!Root->Llink || !Root->Rlink)
		return Root;

	AATREE_LEVEL ShouldBe = MinLevel(Root->Llink->Level, Root->Rlink->Level) + 1;

	if (ShouldBe >= Root->Level)
		return Root;

	Root->Level = ShouldBe;

	if (Root->Rlink->Level > ShouldBe)
		Root->Rlink->Level = ShouldBe;

	return Root;
}

static PAATREE_ENTRY EiSuccessorAaTree(PAATREE_ENTRY Root, PAATREE_ENTRY** SuLink)
{
	if (!Root->Rlink)
		return NULL;

	PAATREE_ENTRY Successor = Root->Rlink;
	*SuLink = &Root->Rlink;

	while (Successor->Llink)
	{
		*SuLink = &Successor->Llink;
		Successor = Successor->Llink;
	}

	return Successor;
}

static PAATREE_ENTRY EiRemoveItemAaTree(PAATREE_ENTRY Root, PAATREE_ENTRY Item, bool* Removed)
{
	if (!Root)
		return Root;

	if (Root->Key != Item->Key)
	{
		if (Item->Key < Root->Key)
			Root->Llink = EiRemoveItemAaTree(Root->Llink, Item, Removed);
		else
			Root->Rlink = EiRemoveItemAaTree(Root->Rlink, Item, Removed);
		goto Rebalance;
	}

	if (EiIsLeafAaTree(Root)) {
		*Removed = true;
		return NULL;
	}

	if (Root->Rlink == NULL) {
		*Removed = true;
		return Root->Llink;
	}

	if (Root->Llink == NULL) {
		*Removed = true;
		return Root->Rlink;
	}

	PAATREE_ENTRY* SuLink;
	PAATREE_ENTRY Successor = EiSuccessorAaTree(Root, &SuLink);

	Successor->Llink = Root->Llink;
	Successor->Rlink = Root->Rlink;

#ifdef DEBUG
	Root->Llink = Root->Rlink = NULL;
#endif

	*SuLink = NULL;

	Root = Successor;
	
	*Removed = true;
	
Rebalance:
	Root = EiDecreaseLevelAaTree(Root);
	Root = EiSkewAaTree(Root);
	Root->Rlink = EiSkewAaTree(Root->Rlink);

	if (Root->Rlink)
		Root->Rlink->Rlink = EiSkewAaTree(Root->Rlink->Rlink);

	Root = EiSplitAaTree(Root);
	Root->Rlink = EiSplitAaTree(Root->Rlink);
	return Root;
}

static void EiTraverseInOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	if (!Root)
		return;

	EiTraverseInOrderAaTree(Root->Llink, Function, Context);
	Function(Context, Root);
	EiTraverseInOrderAaTree(Root->Rlink, Function, Context);
}

static void EiTraversePreOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	if (!Root)
		return;

	Function(Context, Root);
	EiTraversePreOrderAaTree(Root->Llink, Function, Context);
	EiTraversePreOrderAaTree(Root->Rlink, Function, Context);
}

static void EiTraversePostOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	if (!Root)
		return;

	EiTraversePostOrderAaTree(Root->Llink, Function, Context);
	EiTraversePostOrderAaTree(Root->Rlink, Function, Context);
	Function(Context, Root);
}

static PAATREE_ENTRY EiLookUpItemAaTree(PAATREE_ENTRY Root, AATREE_KEY Key)
{
	if (!Root)
		return NULL;

	if (Root->Key == Key)
		return Root;

	if (Key < Root->Key)
		return EiLookUpItemAaTree(Root->Llink, Key);
	else
		return EiLookUpItemAaTree(Root->Rlink, Key);
}

size_t EiGetSizeAaTree(PAATREE_ENTRY Root)
{
	if (!Root)
		return 0;

	return 1 + EiGetSizeAaTree(Root->Llink) + EiGetSizeAaTree(Root->Rlink);
}

static PAATREE_ENTRY EiGetFirstEntryAaTree(PAATREE_ENTRY Root)
{
	if (!Root)
		return NULL;

	while (Root->Llink)
		Root = Root->Llink;

	return Root;
}

static PAATREE_ENTRY EiGetLastEntryAaTree(PAATREE_ENTRY Root)
{
	if (!Root)
		return NULL;

	while (Root->Rlink)
		Root = Root->Rlink;

	return Root;
}

// ------- Exposed API -------

// WRITE
bool ExInsertItemAaTree(PAATREE Tree, PAATREE_ENTRY Item)
{
	bool Inserted = false;
	Tree->Root = EiInsertItemAaTree(Tree->Root, Item, &Inserted);
	return Inserted;
}

bool ExRemoveItemAaTree(PAATREE Tree, PAATREE_ENTRY Item)
{
	bool Removed = false;
	Tree->Root = EiRemoveItemAaTree(Tree->Root, Item, &Removed);
	return Removed;
}

// READ
PAATREE_ENTRY ExLookUpItemAaTree(PAATREE Tree, AATREE_KEY Key)
{
	return EiLookUpItemAaTree(Tree->Root, Key);
}

PAATREE_ENTRY ExGetFirstEntryAaTree(PAATREE Tree)
{
	return EiGetFirstEntryAaTree(Tree->Root);
}

PAATREE_ENTRY ExGetLastEntryAaTree(PAATREE Tree)
{
	return EiGetLastEntryAaTree(Tree->Root);
}

size_t ExGetSizeAaTree(PAATREE Tree)
{
	return EiGetSizeAaTree(Tree->Root);
}

// TRAVERSE
void ExTraverseInOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	EiTraverseInOrderAaTree(Tree->Root, Function, Context);
}

void ExTraversePreOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	EiTraversePreOrderAaTree(Tree->Root, Function, Context);
}

void ExTraversePostOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	EiTraversePostOrderAaTree(Tree->Root, Function, Context);
}
