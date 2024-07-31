/***
	The Boron Operating System
	Copyright (C) 2023-2024 iProgramInCpp

Module name:
	rtl/avltree.h

Abstract:
	This module implements the AVL binary search tree.
	
	Credits to @hyenasky on GitHub: https://github.com/xrarch/mintia/blob/main/OS/OSKernel/Executive/ExAvlTree.df

Author:
	iProgramInCpp - 21 November 2023
***/
#include <rtl/avltree.h>

#define BALANCE_MINUS_ONE (3)

#define GET_PARENT(Node) ((PAVLTREE_ENTRY)((Node)->ParentAndBalance & ~0x3))
#define GET_BALANCE(Node) ((Node)->ParentAndBalance & 0x3)
#define SET_PARENT(Node, Part) ((Node)->ParentAndBalance = ((Node)->ParentAndBalance & 0x3) | ((uintptr_t)(Part) & ~0x3))
#define SET_BALANCE(Node, Bal) ((Node)->ParentAndBalance = ((Node)->ParentAndBalance & ~0x3) | ((unsigned)(Bal) & 0x3))
#define SET_PARENT_AND_BALANCE(Node, Part, Bal) ((Node)->ParentAndBalance = ((uintptr_t)(Part) & ~0x3) | ((unsigned)(Bal) & 0x3))

static const int RtlpBalanceNegator[] = { 0, 3, 2, 1 };

static void RtlpTraverseInOrderAvlTree(
	PAVLTREE_ENTRY Root,
	PAVLTREE_TRAVERSAL_FUNCTION Function,
	void* Context,
	bool* Stop)
{
	if (!Root)
		return;

	if (*Stop) return;

	RtlpTraverseInOrderAvlTree(Root->Llink, Function, Context, Stop);
	if (*Stop) return;

	*Stop = Function(Context, Root);
	if (*Stop) return;

	RtlpTraverseInOrderAvlTree(Root->Rlink, Function, Context, Stop);
}

static void RtlpTraversePreOrderAvlTree(
	PAVLTREE_ENTRY Root,
	PAVLTREE_TRAVERSAL_FUNCTION Function,
	void* Context,
	bool* Stop)
{
	if (!Root)
		return;

	if (*Stop) return;

	*Stop = Function(Context, Root);
	if (*Stop) return;

	RtlpTraversePreOrderAvlTree(Root->Llink, Function, Context, Stop);
	if (*Stop) return;

	RtlpTraversePreOrderAvlTree(Root->Rlink, Function, Context, Stop);
}

static void RtlpTraversePostOrderAvlTree(
	PAVLTREE_ENTRY Root,
	PAVLTREE_TRAVERSAL_FUNCTION Function,
	void* Context,
	bool* Stop)
{
	if (!Root)
		return;

	if (*Stop) return;

	RtlpTraversePostOrderAvlTree(Root->Llink, Function, Context, Stop);
	if (*Stop) return;

	RtlpTraversePostOrderAvlTree(Root->Rlink, Function, Context, Stop);
	if (*Stop) return;

	*Stop = Function(Context, Root);
}

size_t RtlpGetItemCountAvlTree(PAVLTREE_ENTRY Root)
{
	if (!Root)
		return 0;

	return 1 + RtlpGetItemCountAvlTree(Root->Llink) + RtlpGetItemCountAvlTree(Root->Rlink);
}

size_t RtlpGetHeightAvlTree(PAVLTREE_ENTRY Root)
{
	if (!Root)
		return 0;

	size_t Max = RtlpGetHeightAvlTree(Root->Llink);
	size_t RLS = RtlpGetHeightAvlTree(Root->Rlink);

	if (Max < RLS)
		Max = RLS;

	return Max + 1;
}

// Promotes a node up a level.
static void RtlpPromoteAvlTree(PAVLTREE_ENTRY Node)
{
	PAVLTREE_ENTRY Parent, GrandParent, Child;

	Parent = GET_PARENT(Node);
	GrandParent = GET_PARENT(Parent);

	if (Parent->Llink == Node)
	{
		Child = Node->Rlink;
		Parent->Llink = Child;
		if (Child)
			SET_PARENT(Child, Parent);

		Node->Rlink = Parent;
	}
	else
	{
		Child = Node->Llink;
		Parent->Rlink = Child;
		if (Child)
			SET_PARENT(Child, Parent);

		Node->Llink = Parent;
	}

	SET_PARENT(Parent, Node);

	if (GrandParent->Llink == Parent)
		GrandParent->Llink = Node;
	else
		GrandParent->Rlink = Node;

	SET_PARENT(Node, GrandParent);
}

static int /* case */ RtlpRebalanceAvlTree(PAVLTREE_ENTRY Node)
{
	int Balance = GET_BALANCE(Node);
	int ChildBalance;
	PAVLTREE_ENTRY Unbalanced;

	if (Balance == 1)
		Unbalanced = Node->Rlink;
	else
		Unbalanced = Node->Llink;

	ChildBalance = GET_BALANCE(Unbalanced);

	if (ChildBalance == Balance)
	{
		RtlpPromoteAvlTree(Unbalanced);

		// Set balance to zero.
		SET_BALANCE(Node, 0);
		SET_BALANCE(Unbalanced, 0);

		return 0;
	}

	if (ChildBalance == RtlpBalanceNegator[Balance])
	{
		PAVLTREE_ENTRY Child;
		if (Balance == 1)
			Child = Unbalanced->Llink;
		else
			Child = Unbalanced->Rlink;

		RtlpPromoteAvlTree(Child);
		RtlpPromoteAvlTree(Child);

		ChildBalance = GET_BALANCE(Child);
		SET_BALANCE(Child, 0);

		if (ChildBalance == Balance)
		{
			// Set Node's balance to the inverse, set unbalanced's balance to zero.
			SET_BALANCE(Node, RtlpBalanceNegator[Balance]);
			SET_BALANCE(Unbalanced, 0);
		}
		else if (ChildBalance == RtlpBalanceNegator[Balance])
		{
			// Set Unbalanced's balance to the balance, set node's balance to zero.
			SET_BALANCE(Unbalanced, Balance);
			SET_BALANCE(Node, 0);
		}
		else
		{
			// Zero out both balances.
			SET_BALANCE(Node, 0);
			SET_BALANCE(Unbalanced, 0);
		}

		return 0;
	}

	RtlpPromoteAvlTree(Unbalanced);
	SET_BALANCE(Unbalanced, RtlpBalanceNegator[Balance]);
	return 1;
}

// ------- Exposed API -------

// WRITE
bool InsertItemAvlTree(PAVLTREE Tree, PAVLTREE_ENTRY Item)
{
	PAVLTREE_ENTRY FindNode = Tree->Root.Rlink;

	if (!FindNode)
	{
		// Tree is empty or has items only on the left branch.
		// Insert the node directly.
		Tree->Root.Rlink = Item;
		Item->ParentAndBalance = (uintptr_t)&Tree->Root; // balance = 0
		return true;
	}

	AVLTREE_KEY Key = Item->Key;
	PAVLTREE_ENTRY Parent = NULL;
	int ParentBalance;

	while (FindNode)
	{
		Parent = FindNode;

		if (Key < FindNode->Key)
		{
			FindNode = FindNode->Llink;
			ParentBalance = 1;
		}
		else if (Key > FindNode->Key)
		{
			FindNode = FindNode->Rlink;
			ParentBalance = 0;
		}
		else
		{
			//KeCrash("InsertItemAvlTree: Item with same key already exists.");
			//^^ in release builds, will just loop infinitely.

			return false;
		}
	}

	if (ParentBalance)
		Parent->Llink = Item;
	else
		Parent->Rlink = Item;

	SET_PARENT_AND_BALANCE(Item, Parent, 0);

	// Set the root's balance to -1.
	SET_BALANCE(&Tree->Root, BALANCE_MINUS_ONE);

	// Loop until everything's balanced.
	while (true)
	{
		int Balance = 1;

		if (GET_PARENT(Item)->Llink == Item)
			Balance = BALANCE_MINUS_ONE;

		ParentBalance = GET_BALANCE(Parent);

		if (ParentBalance == 0)
		{
			SET_BALANCE(Parent, Balance);

			Item = Parent;
			Parent = GET_PARENT(Parent);
		}
		else if (Balance != ParentBalance)
		{
			SET_BALANCE(Parent, 0);
			break;
		}
		else
		{
			RtlpRebalanceAvlTree(Parent);
			break;
		}
	}

	return true;
}

bool RemoveItemAvlTree(PAVLTREE Tree, PAVLTREE_ENTRY Node)
{
	PAVLTREE_ENTRY SubNode, Parent, Child;

	if (!Node->Llink || !Node->Rlink)
	{
		SubNode = Node;
	}
	else if (GET_BALANCE(Node) == BALANCE_MINUS_ONE)
	{
		// Get predecessor.
		SubNode = Node->Llink;
		while (SubNode->Rlink)
			SubNode = SubNode->Rlink;
	}
	else
	{
		// Get successor.
		SubNode = Node->Rlink;
		while (SubNode->Llink)
			SubNode = SubNode->Llink;
	}

	int Balance = BALANCE_MINUS_ONE;
	Parent = GET_PARENT(SubNode);

	if (!SubNode->Llink)
	{
		Child = SubNode->Rlink;

		if (Parent->Llink == SubNode)
		{
			Parent->Llink = Child;
		}
		else
		{
			Balance = 1;
			Parent->Rlink = Child;
		}

		if (Child)
			SET_PARENT(Child, Parent);
	}
	else
	{
		Child = SubNode->Llink;

		if (Parent->Llink == SubNode)
		{
			Parent->Llink = Child;
		}
		else
		{
			Balance = 1;
			Parent->Rlink = Child;
		}

		SET_PARENT(Child, Parent);
	}

	SET_BALANCE(&Tree->Root, 0);

	// Balance the tree.
	int ParentBalance;
	while (true)
	{
		ParentBalance = GET_BALANCE(Parent);

		if (ParentBalance == Balance)
		{
			SET_BALANCE(Parent, 0);
		}
		else if (ParentBalance == 0)
		{
			// Set the parent's balance to the negated balance.
			SET_BALANCE(Parent, RtlpBalanceNegator[Balance]);
			break;
		}
		else if (RtlpRebalanceAvlTree(Parent))
		{
			break;
		}
		else
		{
			Parent = GET_PARENT(Parent);
		}

		Balance = BALANCE_MINUS_ONE;

		if (GET_PARENT(Parent)->Rlink == Parent)
			Balance = 1;

		Parent = GET_PARENT(Parent);
	}

	if (SubNode != Node)
	{
		SubNode->ParentAndBalance = Node->ParentAndBalance;
		SubNode->Llink = Node->Llink;
		SubNode->Rlink = Node->Rlink;
		Parent = GET_PARENT(SubNode);

		if (GET_PARENT(Node)->Llink == Node)
			Parent->Llink = SubNode;
		else
			Parent->Rlink = SubNode;

		Child = SubNode->Llink;
		if (Child)
			SET_PARENT(Child, SubNode);

		Child = SubNode->Rlink;
		if (Child)
			SET_PARENT(Child, SubNode);
	}

	return true;
}

// READ
PAVLTREE_ENTRY LookUpItemAvlTree(PAVLTREE Tree, AVLTREE_KEY Key)
{
	PAVLTREE_ENTRY Node = Tree->Root.Rlink;

	while (Node)
	{
		if (Key < Node->Key)
			Node = Node->Llink;
		else if (Key > Node->Key)
			Node = Node->Rlink;
		else
			return Node;
	}

	return NULL;
}

PAVLTREE_ENTRY LookUpItemApproximateAvlTree(PAVLTREE Tree, AVLTREE_KEY Key)
{
	PAVLTREE_ENTRY Node = Tree->Root.Rlink;
	PAVLTREE_ENTRY ChosenNode = NULL;

	while (Node)
	{
		if (Key == Node->Key)
		{
			ChosenNode = Node;
			break;
		}
		
		if (Node->Key <= Key)
		{
			ChosenNode = Node;
			Node = Node->Rlink;
			continue;
		}
		
		Node = Node->Llink;
	}

	return ChosenNode;
}

PAVLTREE_ENTRY GetFirstEntryAvlTree(PAVLTREE Tree)
{
	PAVLTREE_ENTRY Node = Tree->Root.Rlink;

	while (Node->Llink)
		Node = Node->Llink;

	return Node;
}

PAVLTREE_ENTRY GetLastEntryAvlTree(PAVLTREE Tree)
{
	PAVLTREE_ENTRY Node = Tree->Root.Rlink;

	while (Node->Rlink)
		Node = Node->Rlink;

	return Node;
}

size_t GetItemCountAvlTree(PAVLTREE Tree)
{
	return RtlpGetItemCountAvlTree(Tree->Root.Rlink);
}

size_t GetHeightAvlTree(PAVLTREE Tree)
{
	return RtlpGetHeightAvlTree(Tree->Root.Rlink);
}

// TRAVERSE
void TraverseInOrderAvlTree(
	PAVLTREE Tree,
	PAVLTREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	bool Stop = false;
	RtlpTraverseInOrderAvlTree(Tree->Root.Rlink, Function, Context, &Stop);
}

void TraversePreOrderAvlTree(
	PAVLTREE Tree,
	PAVLTREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	bool Stop = false;
	RtlpTraversePreOrderAvlTree(Tree->Root.Rlink, Function, Context, &Stop);
}

void TraversePostOrderAvlTree(
	PAVLTREE Tree,
	PAVLTREE_TRAVERSAL_FUNCTION Function,
	void* Context)
{
	bool Stop = false;
	RtlpTraversePostOrderAvlTree(Tree->Root.Rlink, Function, Context, &Stop);
}