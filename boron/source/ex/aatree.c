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

PAATREE_ENTRY ExInsertAaTree(PAATREE_ENTRY Root, PAATREE_ENTRY Item, bool* Inserted)
{
	// Optimistically assume this is the case.
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
		Root->Llink = ExInsertAaTree(Root->Llink, Item, Inserted);
	}
	else if (Item->Key > Root->Key)
	{
		// Perform insertion recursively in the Llink entry.
		Root->Rlink = ExInsertAaTree(Root->Rlink, Item, Inserted);
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

PAATREE_ENTRY ExDeleteAaTree(PAATREE_ENTRY Root, PAATREE_ENTRY Item)
{
	if (!Root)
		return Root;

	if (Root->Key != Item->Key)
	{
		if (Item->Key < Root->Key)
			Root->Llink = ExDeleteAaTree(Root->Llink, Item);
		else
			Root->Rlink = ExDeleteAaTree(Root->Rlink, Item);
		goto Rebalance;
	}

	if (EiIsLeafAaTree(Root))
		return NULL;

	if (Root->Rlink == NULL)
		return Root->Llink;

	if (Root->Llink == NULL)
		return Root->Rlink;

	PAATREE_ENTRY* SuLink;
	PAATREE_ENTRY Successor = EiSuccessorAaTree(Root, &SuLink);

	Successor->Llink = Root->Llink;
	Successor->Rlink = Root->Rlink;

#ifdef DEBUG
	Root->Llink = Root->Rlink = NULL;
#endif

	*SuLink = NULL;

	Root = Successor;

	// TODO: Fix degeneration into a linked list.
	// At least it's currently better than a dumb binary search tree :)

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

void ExTraverseInOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	if (!Root)
		return;

	ExTraverseInOrderAaTree(Root->Llink, Function, Context);
	Function(Context, Root);
	ExTraverseInOrderAaTree(Root->Rlink, Function, Context);
}

void ExTraversePreOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	if (!Root)
		return;

	Function(Context, Root);
	ExTraversePreOrderAaTree(Root->Llink, Function, Context);
	ExTraversePreOrderAaTree(Root->Rlink, Function, Context);
}

void ExTraversePostOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	if (!Root)
		return;

	ExTraversePostOrderAaTree(Root->Llink, Function, Context);
	ExTraversePostOrderAaTree(Root->Rlink, Function, Context);
	Function(Context, Root);
}

PAATREE_ENTRY ExLookUpAaTree(PAATREE_ENTRY Root, AATREE_KEY Key)
{
	if (!Root)
		return NULL;

	if (Root->Key == Key)
		return Root;

	if (Key < Root->Key)
		return ExLookUpAaTree(Root->Llink, Key);
	else
		return ExLookUpAaTree(Root->Rlink, Key);
}

size_t ExGetSizeAaTree(PAATREE_ENTRY Root)
{
	if (!Root)
		return 0;

	return 1 + ExGetSizeAaTree(Root->Llink) + ExGetSizeAaTree(Root->Rlink);
}

PAATREE_ENTRY ExGetFirstEntryAaTree(PAATREE_ENTRY Root)
{
	if (!Root)
		return NULL;

	while (Root->Llink)
		Root = Root->Llink;

	return Root;
}

PAATREE_ENTRY ExGetLastEntryAaTree(PAATREE_ENTRY Root)
{
	if (!Root)
		return NULL;

	while (Root->Rlink)
		Root = Root->Rlink;

	return Root;
}
