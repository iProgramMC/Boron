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
	Container->First = Container->Last = NULL;
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
	KeAcquireSpinLock(&Container->Lock);
	
	// Look for any pre-existing free elements.
	PMISLAB_ITEM Item = Container->First;
	
	while (Item)
	{
		void* Mem = MmpSlabItemTryAllocate(Item, Container->ItemSize);
		if (Mem)
		{
			KeReleaseSpinLock(&Container->Lock);
			return Mem;
		}
		
		Item = Item->Flink;
	}
	
	// Allocate a new slab item.
	int ItemPfn = MmAllocatePhysicalPage();
	if (ItemPfn == PFN_INVALID)
	{
		SLogMsg("MmpSlabContainerAllocate: Run out of memory! What will we do?!");
		// TODO: invoke the out of memory handler here, then try again
		KeReleaseSpinLock(&Container->Lock);
		return NULL;
	}
	
	Item = MmGetHHDMOffsetAddr(MmPFNToPhysPage(ItemPfn));
	memset(Item, 0, sizeof *Item);
	
	Item->Check  = MI_SLAB_ITEM_CHECK;
	Item->Parent = Container;
	
	// Link it to the list:
	if (Container->First)
	{
		Container->First->Blink = Item;
		Item->Flink = Container->First;
		Item->Blink = NULL;
	}
	
	Container->First = Item;
	
	if (!Container->Last)
		Container->Last = Item;
	
	void* Mem = MmpSlabItemTryAllocate(Item, Container->ItemSize);
	KeReleaseSpinLock(&Container->Lock);
	return Mem;
}

void MmpSlabContainerFree(PMISLAB_CONTAINER Container, void* Ptr)
{
	KeAcquireSpinLock(&Container->Lock);
	
	uint8_t* PtrBytes = Ptr;
	
	PMISLAB_ITEM Item = (PMISLAB_ITEM) ((uintptr_t) PtrBytes & ~0xFFF);
	
	if (Item->Check != MI_SLAB_ITEM_CHECK || Item->Parent != Container)
	{
		SLogMsg("Error in MmpSlabContainerFree: Pointer %p isn't actually part of this container!", Ptr);
		KeReleaseSpinLock(&Container->Lock);
		return;
	}
	
	// Get the offset.
	ptrdiff_t Diff = PtrBytes - (uint8_t*)Item;
	if (Diff % Container->ItemSize != 0)
	{
		SLogMsg("Error in MmpSlabContainerFree: Pointer %p was made up", Ptr);
		KeReleaseSpinLock(&Container->Lock);
		return;
	}
	
	Diff /= Container->ItemSize;
	
	// Unset the relevant bit
	Item->Bitmap[Diff / 64] &= ~(1 << (Diff % 64));
	
	// Check if it's all zero:
	if (!Item->Bitmap[0] && !Item->Bitmap[1] && !Item->Bitmap[2] && !Item->Bitmap[3])
	{
		// Free it:
		if (Item->Flink)
			Item->Flink->Blink = Item->Blink;
		if (Item->Blink)
			Item->Blink->Flink = Item->Flink;
		if (Container->First == Item)
			Container->First  = Item->Flink;
		if (Container->Last  == Item)
			Container->Last   = Item->Blink;
		
		int Pfn = MmPhysPageToPFN(MmGetHHDMOffsetFromAddr(Item));
		MmFreePhysicalPage(Pfn);
	}
	
	KeReleaseSpinLock(&Container->Lock);
}

#ifdef MI_SLAB_DEBUG
#define DEBUG_PRINT_THEN_RETURN(Result, Format, ...) do { \
	void* __RESULT = (Result);    \
	SLogMsg(Format, __VA_ARGS__); \
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
	SLogMsg("[%d] MiSlabAllocate(%zu)", SomethingElse, Size);
#endif
	
	int Index = MmGetSmallestPO2ThatFitsSize(Size);
	
	if (Index >= MISLAB_SIZE_COUNT)
	{
		SLogMsg("Error, MiSlabAllocate(%zu) is not supported", Size);
		return NULL;
	}
	
	DEBUG_PRINT_THEN_RETURN(MmpSlabContainerAllocate(&MiSlabContainer[Index]), "[%d] => %p", SomethingElse, __RESULT);
}

void MiSlabFree(void* Ptr, size_t Size)
{
#ifdef MI_SLAB_DEBUG
	static int Something;
	int SomethingElse = AtAddFetch(Something, 1);
	SLogMsg("[%d] MiSlabFree(%p, %zu)", SomethingElse, Ptr, Size);
#endif

	int Index = MmGetSmallestPO2ThatFitsSize(Size);
	
	if (Index >= MISLAB_SIZE_COUNT)
	{
		SLogMsg("Error, MiSlabFree(%p, %zu) is not supported", Ptr, Size);
		return;
	}
	
	MmpSlabContainerFree(&MiSlabContainer[Index], Ptr);
	
#ifdef MI_SLAB_DEBUG
	SLogMsg("[%d] Done", SomethingElse);
#endif
}
