/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	rtl/aatree.h

Abstract:
	This module implements the AA binary search tree.

Author:
	iProgramInCpp - 21 November 2023
***/
#include <rtl/aatree.h>

static PAATREE_ENTRY RtlpSkewAaTree(PAATREE_ENTRY Root)
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

static PAATREE_ENTRY RtlpSplitAaTree(PAATREE_ENTRY Root)
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

static PAATREE_ENTRY RtlpInsertItemAaTree(PAATREE_ENTRY Root, PAATREE_ENTRY Item, bool* Inserted, int Depth)
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
		Root->Llink = RtlpInsertItemAaTree(Root->Llink, Item, Inserted, Depth + 1);
	}
	else if (Item->Key > Root->Key)
	{
		// Perform insertion recursively in the Llink entry.
		Root->Rlink = RtlpInsertItemAaTree(Root->Rlink, Item, Inserted, Depth + 1);
	}
	else
	{
		// Item with the same key already exists.
		*Inserted = false;
	}

	// Perform a skew and split. If not inserted, don't bother.
	if (*Inserted)
	{
		Root = RtlpSkewAaTree(Root);
		Root = RtlpSplitAaTree(Root);
	}

	// Return the new root.
	return Root;
}

static bool RtlpIsLeafAaTree(PAATREE_ENTRY Item)
{
	return Item->Llink == NULL && Item->Rlink == NULL;
}

static AATREE_LEVEL MinLevel(
	AATREE_LEVEL a,
	AATREE_LEVEL b)
{
	return a < b ? a : b;
}

static PAATREE_ENTRY RtlpDecreaseLevelAaTree(PAATREE_ENTRY Root)
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

static PAATREE_ENTRY RtlpSuccessorAaTree(PAATREE_ENTRY Root, PAATREE_ENTRY** SuLink)
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

static PAATREE_ENTRY RtlpPredecessorAaTree(PAATREE_ENTRY Root, PAATREE_ENTRY** SuLink)
{
	if (!Root->Llink)
		return NULL;

	PAATREE_ENTRY Predecessor = Root->Llink;
	*SuLink = &Root->Llink;

	while (Predecessor->Rlink)
	{
		*SuLink = &Predecessor->Rlink;
		Predecessor = Predecessor->Rlink;
	}

	return Predecessor;
}

static inline ALWAYS_INLINE
void SwapLinks(PAATREE_ENTRY* Link1, PAATREE_ENTRY* Link2)
{
	PAATREE_ENTRY Temp = *Link1;
	*Link1 = *Link2;
	*Link2 = Temp;
}

static inline ALWAYS_INLINE
void SwapKeys(AATREE_KEY* Link1, AATREE_KEY* Link2)
{
	AATREE_KEY Temp = *Link1;
	*Link1 = *Link2;
	*Link2 = Temp;
}

static PAATREE_ENTRY RtlpRemoveItemAaTree(PAATREE_ENTRY Root, PAATREE_ENTRY Item, bool* Removed)
{
	if (!Root)
		return Root;

	if (Root->Key != Item->Key)
	{
		if (Item->Key < Root->Key)
			Root->Llink = RtlpRemoveItemAaTree(Root->Llink, Item, Removed);
		else
			Root->Rlink = RtlpRemoveItemAaTree(Root->Rlink, Item, Removed);

		goto Rebalance;
	}

	if (RtlpIsLeafAaTree(Root)) {
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
	if (Root->Llink == NULL)
	{
		PAATREE_ENTRY Successor = RtlpSuccessorAaTree(Root, &SuLink);

		*SuLink = Root;

		SwapLinks(&Root->Llink, &Successor->Llink);
		SwapLinks(&Root->Rlink, &Successor->Rlink);

		bool bRemoved = false;
		Successor->Llink = RtlpRemoveItemAaTree(Successor->Llink, Root, &bRemoved);

		Root = Successor;
	}
	else
	{
		PAATREE_ENTRY Predecessor = RtlpPredecessorAaTree(Root, &SuLink);

		*SuLink = Root;

		SwapLinks(&Root->Llink, &Predecessor->Llink);
		SwapLinks(&Root->Rlink, &Predecessor->Rlink);

		bool bRemoved = false;
		Predecessor->Llink = RtlpRemoveItemAaTree(Predecessor->Llink, Root, &bRemoved);

		Root = Predecessor;
	}

Rebalance:
	Root = RtlpDecreaseLevelAaTree(Root);
	Root = RtlpSkewAaTree(Root);
	Root->Rlink = RtlpSkewAaTree(Root->Rlink);

	if (Root->Rlink)
		Root->Rlink->Rlink = RtlpSkewAaTree(Root->Rlink->Rlink);

	Root = RtlpSplitAaTree(Root);
	Root->Rlink = RtlpSplitAaTree(Root->Rlink);
	return Root;
}

static void RtlpTraverseInOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context,
	bool* Stop)
{
	if (!Root)
		return;

	if (*Stop) return;

	RtlpTraverseInOrderAaTree(Root->Llink, Function, Context, Stop);
	if (*Stop) return;

	*Stop = Function(Context, Root);
	if (*Stop) return;

	RtlpTraverseInOrderAaTree(Root->Rlink, Function, Context, Stop);
}

static void RtlpTraversePreOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context,
	bool* Stop)
{
	if (!Root)
		return;

	if (*Stop) return;

	*Stop = Function(Context, Root);
	if (*Stop) return;

	RtlpTraversePreOrderAaTree(Root->Llink, Function, Context, Stop);
	if (*Stop) return;

	RtlpTraversePreOrderAaTree(Root->Rlink, Function, Context, Stop);
}

static void RtlpTraversePostOrderAaTree(
	PAATREE_ENTRY Root,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context,
	bool* Stop)
{
	if (!Root)
		return;

	if (*Stop) return;

	RtlpTraversePostOrderAaTree(Root->Llink, Function, Context, Stop);
	if (*Stop) return;

	RtlpTraversePostOrderAaTree(Root->Rlink, Function, Context, Stop);
	if (*Stop) return;

	*Stop = Function(Context, Root);
}

static PAATREE_ENTRY RtlpLookUpItemAaTree(PAATREE_ENTRY Root, AATREE_KEY Key)
{
	if (!Root)
		return NULL;

	if (Root->Key == Key)
		return Root;

	if (Key < Root->Key)
		return RtlpLookUpItemAaTree(Root->Llink, Key);
	else
		return RtlpLookUpItemAaTree(Root->Rlink, Key);
}

size_t RtlpGetItemCountAaTree(PAATREE_ENTRY Root)
{
	if (!Root)
		return 0;

	return 1 + RtlpGetItemCountAaTree(Root->Llink) + RtlpGetItemCountAaTree(Root->Rlink);
}

size_t RtlpGetHeightAaTree(PAATREE_ENTRY Root)
{
	if (!Root)
		return 0;

	size_t Max = RtlpGetHeightAaTree(Root->Llink);
	size_t RLS = RtlpGetHeightAaTree(Root->Rlink);

	if (Max < RLS)
		Max = RLS;

	return Max + 1;
}

static PAATREE_ENTRY RtlpGetFirstEntryAaTree(PAATREE_ENTRY Root)
{
	if (!Root)
		return NULL;

	while (Root->Llink)
		Root = Root->Llink;

	return Root;
}

static PAATREE_ENTRY RtlpGetLastEntryAaTree(PAATREE_ENTRY Root)
{
	if (!Root)
		return NULL;

	while (Root->Rlink)
		Root = Root->Rlink;

	return Root;
}

// ------- Exposed API -------

// WRITE
bool InsertItemAaTree(PAATREE Tree, PAATREE_ENTRY Item)
{
	bool Inserted = false;
	Tree->Root = RtlpInsertItemAaTree(Tree->Root, Item, &Inserted, 0);
	return Inserted;
}

bool RemoveItemAaTree(PAATREE Tree, PAATREE_ENTRY Item)
{
	bool Removed = false;
	Tree->Root = RtlpRemoveItemAaTree(Tree->Root, Item, &Removed);
	return Removed;
}

// READ
PAATREE_ENTRY LookUpItemAaTree(PAATREE Tree, AATREE_KEY Key)
{
	return RtlpLookUpItemAaTree(Tree->Root, Key);
}

PAATREE_ENTRY GetFirstEntryAaTree(PAATREE Tree)
{
	return RtlpGetFirstEntryAaTree(Tree->Root);
}

PAATREE_ENTRY GetLastEntryAaTree(PAATREE Tree)
{
	return RtlpGetLastEntryAaTree(Tree->Root);
}

size_t GetItemCountAaTree(PAATREE Tree)
{
	return RtlpGetItemCountAaTree(Tree->Root);
}

size_t GetHeightAaTree(PAATREE Tree)
{
	return RtlpGetHeightAaTree(Tree->Root);
}

// TRAVERSE
void TraverseInOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	bool Stop = false;
	RtlpTraverseInOrderAaTree(Tree->Root, Function, Context, &Stop);
}

void TraversePreOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	bool Stop = false;
	RtlpTraversePreOrderAaTree(Tree->Root, Function, Context, &Stop);
}

void TraversePostOrderAaTree(
	PAATREE Tree,
	PAATREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	bool Stop = false;
	RtlpTraversePostOrderAaTree(Tree->Root, Function, Context, &Stop);
}
