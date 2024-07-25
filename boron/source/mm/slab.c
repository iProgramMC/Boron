/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/slab.c
	
Abstract:
	This is the implementation of the slab allocator.
	
Author:
	iProgramInCpp - 24 September 2023
***/
#include "mi.h"

#ifdef DEBUG2
#define MI_SLAB_DEBUG
#endif

static const int MiSlabSizes[] =
{
	sizeof(KTHREAD),
	sizeof(KPROCESS),
	8,
	16,
	32,
	64,
	128,
	256,
	512,
	1024,
	2048,
	4096,
	8192,
	12288,
	16384,
	20480,
	24576,
	28672,
	32768,
	0,
};

static MISLAB_CONTAINER MiSlabContainer[2][MISLAB_SIZE_COUNT];

// This tree is used when a slab item larger than a page size is allocated.
//
// TODO: There is probably going to be a ton of contention on this lock, we need something better.
KSPIN_LOCK MmpSlabItemTopLock;
AVLTREE    MmpSlabItemTopTree;

static size_t MmpSlabItemDetermineLength(int ItemSize)
{
	if (ItemSize < PAGE_SIZE / 4)
		return PAGE_SIZE;
	
	// Allow at least 4 items to fit.
	return (ItemSize * 4 + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;
}

static bool MmpRequiresAvlTreeEntry(int ItemSize)
{
	return ItemSize >= PAGE_SIZE / 4;
}

static bool MmpAddSlabItemToTree(PMISLAB_ITEM SlabItem)
{
	AVLTREE_KEY Key = (AVLTREE_KEY)(uintptr_t)SlabItem;
	InitializeAvlTreeEntry(&SlabItem->TreeEntry);
	SlabItem->TreeEntry.Key = Key;
	
	KIPL Ipl;
	KeAcquireSpinLock(&MmpSlabItemTopLock, &Ipl);
	
	bool Inserted = InsertItemAvlTree(&MmpSlabItemTopTree, &SlabItem->TreeEntry);
	
	KeReleaseSpinLock(&MmpSlabItemTopLock, Ipl);
	return Inserted;
}

static bool MmpRemoveSlabItemFromTree(PMISLAB_ITEM SlabItem)
{
	KIPL Ipl;
	KeAcquireSpinLock(&MmpSlabItemTopLock, &Ipl);
	ASSERT(SlabItem->TreeEntry.Key == (AVLTREE_KEY)(uintptr_t)SlabItem);
	
	bool Removed = RemoveItemAvlTree(&MmpSlabItemTopTree, &SlabItem->TreeEntry);
	
	KeReleaseSpinLock(&MmpSlabItemTopLock, Ipl);
	return Removed;
}

static void MmpInitSlabContainer(PMISLAB_CONTAINER Container, int Size, bool NonPaged)
{
	Container->ItemSize = Size;
	Container->NonPaged = NonPaged;
	Container->Lock.Locked = false;
	InitializeListHead(&Container->ListHead);
}

void MiInitSlabs()
{
	for (int i = 0; i < MISLAB_SIZE_COUNT; i++)
	{
		MmpInitSlabContainer(&MiSlabContainer[0][i], MiSlabSizes[i], false);
		MmpInitSlabContainer(&MiSlabContainer[1][i], MiSlabSizes[i], true);
	}
}

int MmGetSmallestSlabSizeThatFitsSize(size_t Size)
{
	int i;
	// Check for exact matches for common kernel structures.
	for (i = MISLAB_EXACT_START; i < MISLAB_EXACT_LIMIT; i++)
	{
		if ((size_t)MiSlabSizes[i] == Size)
			return i;
	}
	
	// Check for generic matches.
	for (i = MISLAB_GENERIC_START; i < MISLAB_GENERIC_LIMIT; i++)
	{
		if ((size_t)MiSlabSizes[i] >= Size)
			break;
	}
	return i;
}

void* MmpSlabItemTryAllocate(PMISLAB_ITEM Item, int EntrySize)
{
	int EntriesPerItem     = (Item->Length - sizeof(Item)) / EntrySize;
	int BitmapWrdsToCheck  = (EntriesPerItem + 63) / 64;
	int LastBitmapBitCount = EntriesPerItem % 64;
	
	// NOTE: If the slab item has an AVL tree index into it, it can only store
	// at most 4 items inside, so therefore only one of the bitmaps will actually
	// be checked.
	
	for (int BitmapIndex = 0; BitmapIndex < BitmapWrdsToCheck; BitmapIndex++)
	{
		int Count = 64;
		if (BitmapIndex == BitmapWrdsToCheck - 1 && LastBitmapBitCount)
			Count = LastBitmapBitCount;
		
		uint64_t BitMap = Item->Bitmap[BitmapIndex];
		
		for (int Index = 0; Index < Count; Index++)
		{
			if (~BitMap & (1 << Index))
			{
				Item->Bitmap[BitmapIndex] |= 1 << Index;
				
				int Index2 = (64 * BitmapIndex + Index) * EntrySize;
				
				return &Item->Data[Index2];
			}
		}
	}
	
	return NULL;
}

void* MmpSlabContainerAllocate(PMISLAB_CONTAINER Container)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&Container->Lock, &OldIpl);
	
	// Look for any pre-existing free elements.
	PLIST_ENTRY ItemEntry = Container->ListHead.Flink;
	
	while (ItemEntry != &Container->ListHead)
	{
		PMISLAB_ITEM Item = CONTAINING_RECORD(ItemEntry, MISLAB_ITEM, ListEntry);
		
		void* Mem = MmpSlabItemTryAllocate(Item, Container->ItemSize);
		if (Mem)
		{
			KeReleaseSpinLock(&Container->Lock, OldIpl);
			memset(Mem, 0, Container->ItemSize);
			return Mem;
		}
		
		ItemEntry = ItemEntry->Flink;
	}
	
	// Allocate a new slab item.
	PMISLAB_ITEM Item;
	void* Addr;
	
	int Length = MmpSlabItemDetermineLength(Container->ItemSize);
	
	// TODO: Fix that since we're locking a spinlock, we can't take
	// page faults on that memory.  Even if we mapped it all already,
	// it's going to cause issues when freeing too..
	
	BIG_MEMORY_HANDLE Handle = MmAllocatePoolBig(
		//Container->NonPaged ? POOL_FLAG_NON_PAGED : 0,
		POOL_FLAG_NON_PAGED,
		(Length + PAGE_SIZE - 1) / PAGE_SIZE,
		&Addr,
		POOL_TAG("SbIt")
	);
	
	if (!Handle)
	{
		DbgPrint("ERROR: MmpSlabContainerAllocate: Out of memory! What will we do?!");
		// TODO
		KeReleaseSpinLock(&Container->Lock, OldIpl);
		return NULL;
	}
	
	Item = Addr;
	
	memset(Item, 0, sizeof *Item);
	
	Item->Check  = MI_SLAB_ITEM_CHECK;
	Item->Parent = Container;
	Item->Length = Length;
	
	Item->MemHandle = Handle;
	
	// Link it to the list:
	InsertTailList(&Container->ListHead, &Item->ListEntry);
	
	// If the entry size is big enough, we're going to want to insert it into the AVL tree too.
	if (MmpRequiresAvlTreeEntry(Container->ItemSize))
		MmpAddSlabItemToTree(Item);
	
	void* Mem = MmpSlabItemTryAllocate(Item, Container->ItemSize);
	ASSERT(Mem);
	memset(Mem, 0, Container->ItemSize);
	KeReleaseSpinLock(&Container->Lock, OldIpl);
	return Mem;
}

void MmpSlabContainerFree(PMISLAB_CONTAINER Container, PMISLAB_ITEM Item, void* Ptr)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&Container->Lock, &OldIpl);
	
	uint8_t* PtrBytes = Ptr;
	
	if (Item->Check != MI_SLAB_ITEM_CHECK || Item->Parent != Container)
	{
		DbgPrint("Error in MmpSlabContainerFree: Pointer %p isn't actually part of this container!", Ptr);
		KeReleaseSpinLock(&Container->Lock, OldIpl);
		return;
	}
	
	// Get the offset.
	ptrdiff_t Diff = PtrBytes - (uint8_t*)Item->Data;
	if (Diff % Container->ItemSize != 0)
	{
		DbgPrint("Error in MmpSlabContainerFree: Pointer %p was made up", Ptr);
		KeReleaseSpinLock(&Container->Lock, OldIpl);
		return;
	}
	
	Diff /= Container->ItemSize;
	
	// Unset the relevant bit
	Item->Bitmap[Diff / 64] &= ~(1 << (Diff % 64));
	
	bool RequiresAvlTreeEntry = MmpRequiresAvlTreeEntry(Container->ItemSize);
	
	// Check if it's all zero:
	if (!Item->Bitmap[0] && (RequiresAvlTreeEntry || (!Item->Bitmap[1] && !Item->Bitmap[2] && !Item->Bitmap[3])))
	{
		RemoveEntryList(&Item->ListEntry);
		
		if (RequiresAvlTreeEntry)
			MmpRemoveSlabItemFromTree(Item);
		
		// Free the memory handle.
		MmFreePoolBig(Item->MemHandle);
	}
	
	KeReleaseSpinLock(&Container->Lock, OldIpl);
}

void* MmpAllocateHuge(bool IsNonPaged, size_t Size)
{	
	size_t PageCount = (Size + sizeof(HUGE_MEMORY_BLOCK) + PAGE_SIZE - 1) / PAGE_SIZE;
	
	void* Addr;
	BIG_MEMORY_HANDLE MemHandle = MmAllocatePoolBig(
		IsNonPaged ? POOL_FLAG_NON_PAGED : 0,
		PageCount,
		&Addr,
		POOL_TAG("BHUG")
	);
	
	if (!MemHandle)
	{
		ASSERT(!"Shit!");
		return NULL;
	}
	
	PHUGE_MEMORY_BLOCK Hmb = Addr;
	Hmb->MemHandle = MemHandle;
	Hmb->Check = MI_HUGE_MEMORY_CHECK;
	memset(Hmb->Data, 0, Size);
	
	return Hmb->Data;
}

void MmpFreeHuge(PHUGE_MEMORY_BLOCK Hmb)
{
	MmFreePoolBig(Hmb->MemHandle);
}

void* MiSlabAllocate(bool IsNonPaged, size_t Size)
{
	int Index = MmGetSmallestSlabSizeThatFitsSize(Size);
	
	if (Index >= MISLAB_SIZE_COUNT)
	{
		return MmpAllocateHuge(IsNonPaged, Size);
	}
	
	return MmpSlabContainerAllocate(&MiSlabContainer[IsNonPaged][Index]);
}

void MiSlabFree(void* Ptr)
{
	void* PageAlignedPtr = (void*)((uintptr_t)Ptr & ~(PAGE_SIZE - 1));
	
	// Test for huge memory blocks.
	PHUGE_MEMORY_BLOCK Hmb = PageAlignedPtr;
	if (Hmb->Check == MI_HUGE_MEMORY_CHECK)
	{
		// Free it as a huge memory block.
		MmpFreeHuge(Hmb);
		return;
	}
	
	PMISLAB_ITEM Item = PageAlignedPtr;
	
	KIPL Ipl;
	KeAcquireSpinLock(&MmpSlabItemTopLock, &Ipl);
	
	// Check if the slab item top tree has it.
	//
	// If the entry is not NULL, it will point to a valid slab item. But we should also check
	// if the pointer in question belongs inside of that item. If it does, call MmpSlabContainerFree
	// on the tree thing, otherwise, hope that this is a standard small item.
	PAVLTREE_ENTRY Entry = LookUpItemApproximateAvlTree(&MmpSlabItemTopTree, (uintptr_t) PageAlignedPtr);
	
	// NOTE: We don't want to delay releasing the tree lock. The entry should stick around until
	// the free is committed, otherwise we have a double-free and I clearly won't define that.
	KeReleaseSpinLock(&MmpSlabItemTopLock, Ipl);
	
	if (!Entry)
		// There are no entries in the slab item top tree. This means that no allocations whose slab items
		// couldn't fit inside of one page have been issued. Therefore, handle it as if it were a standard
		// small item.
		goto StandardHandling;
	
	// Well, it was, get the pointer to that item.
	Item = CONTAINING_RECORD(Entry, MISLAB_ITEM, TreeEntry);
	
	// This item pointer is page aligned.
	ASSERT(((uintptr_t)Item & 0xFFF) == 0);
	
	// Now check if the pointer we were passed actually belongs inside this slab item.
	if ((char*)Ptr < Item->Data || Item->Data + Item->Length - sizeof(Item) <= (char*)Ptr)
	{
		// No, so we will want to handle it the normal way.
		Item = PageAlignedPtr;
	}
	
StandardHandling:
	ASSERT(Item->Check == MI_SLAB_ITEM_CHECK);
	
	MmpSlabContainerFree(Item->Parent, Item, Ptr);
}
