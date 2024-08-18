/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/cache.h
	
Abstract:
	This header defines the I/O manager's cache control
	block structure.
	
Author:
	iProgramInCpp - 18 August 2024
***/
#pragma once

#include <ke.h>

#define MM_CCB_MUTEX_LEVEL    1

#define MM_INDIRECTION_LEVELS 4
#define MM_DIRECT_PAGE_COUNT  (16 - MM_INDIRECTION_LEVELS)

// Levels of indirection and the amount of extra bytes it would help access, in addition to previous levels:
// L0:  48 KiB (if MM_INDIRECTION_LEVELS==4), 44 KiB (if MM_INDIRECTION_LEVELS == 5)
// L1:   2 MiB
// L2:   1 GiB
// L3: 512 GiB
// L4: 256 TiB
// L5: 128 PiB

// I will go for 4 levels of indirection for now.

typedef struct _FCB FCB, *PFCB;
typedef struct _CCB_INDIRECTION CCB_INDIRECTION, *PCCB_INDIRECTION;

static_assert(sizeof(MMPFN) == sizeof(int), "If you expanded the PFN type to >32 bits, change the CCB entry!");

typedef union _CCB_ENTRY
{
	PCCB_INDIRECTION Indirection;
	
	struct
	{
		uint32_t  Modified : 1;
		uint32_t  Mapped   : 1;
		uint32_t  Spare    : 30;
		uint32_t  Pfn      : 32;
	}
	PACKED U;
}
CCB_ENTRY, *PCCB_ENTRY;

static_assert(sizeof(CCB_ENTRY) == sizeof(uintptr_t));

struct _CCB_INDIRECTION
{
	CCB_ENTRY Entries[PAGE_SIZE / sizeof(CCB_ENTRY)];
};

static_assert(sizeof(CCB_INDIRECTION) == PAGE_SIZE);

typedef struct _CCB
{
	KMUTEX Mutex;
	
	uint64_t FirstModifiedPage;
	uint64_t LastModifiedPage;
	
	MMPTE Direct[MM_DIRECT_PAGE_COUNT];
	PCCB_INDIRECTION Level1Indirect;
	PCCB_INDIRECTION Level2Indirect;
	PCCB_INDIRECTION Level3Indirect;
	PCCB_INDIRECTION Level4Indirect;
#if MM_INDIRECTION_LEVELS == 5
	PCCB_INDIRECTION Level5Indirect;
#endif
}
CCB, *PCCB;

void MmInitializeCcb(PCCB Ccb);

