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

static MISLAB_CONTAINER MiSlabContainer[MISLAB_SIZE_COUNT];

static void MmpInitSlabContainer(PMISLAB_CONTAINER Container, int Size)
{
	Container->ItemSize = Size;
	Container->Lock.Locked = false;
	InitializeListHead(&Container->ListHead);
}

void MiInitSlabs()
{
	for (int i = 0; i < MISLAB_SIZE_COUNT; i++)
	{
		MmpInitSlabContainer(&MiSlabContainer[i], 16 << i);
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
	int ItemPfn = MmAllocatePhysicalPage();
	if (ItemPfn == PFN_INVALID)
	{
		DbgPrint("MmpSlabContainerAllocate: Run out of memory! What will we do?!");
		// TODO: invoke the out of memory handler here, then try again
		KeReleaseSpinLock(&Container->Lock, OldIpl);
		return NULL;
	}
	
	PMISLAB_ITEM Item = MmGetHHDMOffsetAddr(MmPFNToPhysPage(ItemPfn));
	memset(Item, 0, sizeof *Item);
	
	Item->Check  = MI_SLAB_ITEM_CHECK;
	Item->Parent = Container;
	
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
		
		int Pfn = MmPhysPageToPFN(MmGetHHDMOffsetFromAddr(Item));
		MmFreePhysicalPage(Pfn);
	}
	
	KeReleaseSpinLock(&Container->Lock, OldIpl);
}

#ifdef MI_SLAB_DEBUG
#define DEBUG_PRINT_THEN_RETURN(Result, Format, ...) do { \
	void* __RESULT = (Result);    \
	DbgPrint(Format, __VA_ARGS__); \
	return __RESULT;              \
} while (0)
#else
#define DEBUG_PRINT_THEN_RETURN(Result, Unused, ...) do \
	return (Result); \
while (0)
#endif

void* MiSlabAllocate(size_t Size)
{
#ifdef MI_SLAB_DEBUG
	static int Something;
	int SomethingElse = AtAddFetch(Something, 1);
	DbgPrint("[%d] MiSlabAllocate(%zu)", SomethingElse, Size);
#endif
	
	int Index = MmGetSmallestPO2ThatFitsSize(Size);
	
	if (Index >= MISLAB_SIZE_COUNT)
	{
		DbgPrint("Error, MiSlabAllocate(%zu) is not supported", Size);
		return NULL;
	}
	
	DEBUG_PRINT_THEN_RETURN(MmpSlabContainerAllocate(&MiSlabContainer[Index]), "[%d] => %p", SomethingElse, __RESULT);
}

void MiSlabFree(void* Ptr, size_t Size)
{
#ifdef MI_SLAB_DEBUG
	static int Something;
	int SomethingElse = AtAddFetch(Something, 1);
	DbgPrint("[%d] MiSlabFree(%p, %zu)", SomethingElse, Ptr, Size);
#endif

	int Index = MmGetSmallestPO2ThatFitsSize(Size);
	
	if (Index >= MISLAB_SIZE_COUNT)
	{
		DbgPrint("Error, MiSlabFree(%p, %zu) is not supported", Ptr, Size);
		return;
	}
	
	MmpSlabContainerFree(&MiSlabContainer[Index], Ptr);
	
#ifdef MI_SLAB_DEBUG
	DbgPrint("[%d] Done", SomethingElse);
#endif
}
