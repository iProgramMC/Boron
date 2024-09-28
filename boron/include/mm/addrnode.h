/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/addrnode.h
	
Abstract:
	This header defines the address node structure. It is used by both the heap
	manager and the virtual address descriptor manager.
	
Author:
	iProgramInCpp - 22 September 2024
***/
#pragma once

#include <rtl/rbtree.h>

typedef struct
{
	union {
		RBTREE_ENTRY Entry;
		struct {
			// these correspond to the three rbe_link members of the tree
			uintptr_t A, B, C;
			uintptr_t StartVa;
		};
	};

	// VAD/heap node common fields
	size_t Size;
}
MMADDRESS_NODE, *PMMADDRESS_NODE;

#define Node_EndVa(Node) ((Node)->StartVa + (Node)->Size * PAGE_SIZE)

static_assert(offsetof(MMADDRESS_NODE, StartVa) == offsetof(MMADDRESS_NODE, Entry.Key), "these should be the same offset");
