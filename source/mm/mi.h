/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/mi.h
	
Abstract:
	This is the internal header file for the Memory Manager.
	
Author:
	iProgramInCpp - 12 September 2023
***/

#ifndef NS64_MI_H
#define NS64_MI_H

#include <mm.h>
#include <string.h>
#include <hal.h>
#include <ke.h>
#include <_limine.h>

struct MISLAB_CONTAINER_tag;

#define MI_SLAB_ITEM_CHECK (0x424C534E4F524F42)

typedef struct MISLAB_ITEM_tag
{
	char     Data[4096 - 32 - 32];
	
	uint64_t Bitmap[4]; // Supports down to 16 byte sized items
	
	struct MISLAB_ITEM_tag *Flink, *Blink;
	struct MISLAB_CONTAINER_tag *Parent;
	
	uint64_t Check;
}
MISLAB_ITEM, *PMISLAB_ITEM;

typedef struct MISLAB_CONTAINER_tag
{
	int          ItemSize;
	SpinLock     Lock;
	PMISLAB_ITEM First, Last;
}
MISLAB_CONTAINER, *PMISLAB_CONTAINER;

typedef enum MISLAB_SIZE_tag
{
	MISLAB_SIZE_16,
	MISLAB_SIZE_32,
	MISLAB_SIZE_64,
	MISLAB_SIZE_128,
	MISLAB_SIZE_256,
	MISLAB_SIZE_512,
	MISLAB_SIZE_1024,
	MISLAB_SIZE_COUNT,
}
MISLAB_SIZE;

_Static_assert(sizeof(MISLAB_ITEM) <= 4096, "This structure needs to fit inside one page.");

void* MiSlabAllocate(size_t size);
void  MiSlabFree(void* ptr, size_t size);
void  MiInitSlabs();

#endif//NS64_MI_H