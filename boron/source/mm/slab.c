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

static MISLAB_CONTAINER MiSlabContainer[2][MISLAB_SIZE_COUNT];

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
		MmpInitSlabContainer(&MiSlabContainer[0][i], 16 << i, false);
		MmpInitSlabContainer(&MiSlabContainer[1][i], 16 << i, true);
	}
}

int MmGetSmallestPO2ThatFitsSize(size_t Size)
{
	size_t MaxSize = 16;
	int Index = 0;
	
	while (MaxSize < Size)
		MaxSize <<= 1, Index++;
	
	return Index;
}

void* MmpSlabItemTryAllocate(PMISLAB_ITEM Item, int EntrySize)
{
	int EntriesPerItem     = sizeof (Item->Data) / EntrySize;
	int BitmapWrdsToCheck  = (EntriesPerItem + 63) / 64;
	int LastBitmapBitCount = EntriesPerItem % 64;
	
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
			return Mem;
		}
		
		ItemEntry = ItemEntry->Flink;
	}
	
	// Allocate a new slab item.
	PMISLAB_ITEM Item;
	void* Addr;
	
	BIG_MEMORY_HANDLE Handle = MmAllocatePoolBig(
		Container->NonPaged ? POOL_FLAG_NON_PAGED : 0,
		1, // TODO
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
	
	Item->MemHandle = Handle;
	
	// Link it to the list:
	InsertTailList(&Container->ListHead, &Item->ListEntry);
	
	void* Mem = MmpSlabItemTryAllocate(Item, Container->ItemSize);
	KeReleaseSpinLock(&Container->Lock, OldIpl);
	return Mem;
}

void MmpSlabContainerFree(PMISLAB_CONTAINER Container, void* Ptr)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&Container->Lock, &OldIpl);
	
	uint8_t* PtrBytes = Ptr;
	
	PMISLAB_ITEM Item = (PMISLAB_ITEM) ((uintptr_t) PtrBytes & ~0xFFF);
	
	if (Item->Check != MI_SLAB_ITEM_CHECK || Item->Parent != Container)
	{
		DbgPrint("Error in MmpSlabContainerFree: Pointer %p isn't actually part of this container!", Ptr);
		KeReleaseSpinLock(&Container->Lock, OldIpl);
		return;
	}
	
	// Get the offset.
	ptrdiff_t Diff = PtrBytes - (uint8_t*)Item;
	if (Diff % Container->ItemSize != 0)
	{
		DbgPrint("Error in MmpSlabContainerFree: Pointer %p was made up", Ptr);
		KeReleaseSpinLock(&Container->Lock, OldIpl);
		return;
	}
	
	Diff /= Container->ItemSize;
	
	// Unset the relevant bit
	Item->Bitmap[Diff / 64] &= ~(1 << (Diff % 64));
	
	// Check if it's all zero:
	if (!Item->Bitmap[0] && !Item->Bitmap[1] && !Item->Bitmap[2] && !Item->Bitmap[3])
	{
		RemoveEntryList(&Item->ListEntry);
		
		// Free the memory handle.
		MmFreePoolBig(Item->MemHandle);
	}
	
	KeReleaseSpinLock(&Container->Lock, OldIpl);
}

void* MiSlabAllocate(bool IsNonPaged, size_t Size)
{
	int Index = MmGetSmallestPO2ThatFitsSize(Size);
	
	if (Index >= MISLAB_SIZE_COUNT)
	{
		DbgPrint("Error, MiSlabAllocate(%zu) is not supported", Size);
		return NULL;
	}
	
	return MmpSlabContainerAllocate(&MiSlabContainer[IsNonPaged][Index]);
}

void MiSlabFree(void* Ptr)
{
	PMISLAB_ITEM Item = (PMISLAB_ITEM)((uintptr_t)Ptr & ~(PAGE_SIZE - 1));
	
	MmpSlabContainerFree(Item->Parent, Ptr);
}
