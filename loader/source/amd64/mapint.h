/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                          mapint.h                           *
\*************************************************************/
#pragma once

#include <loader.h>
#include "../mapper.h"
#include "../requests.h"

// Page table entry bits
#define MM_PTE_PRESENT    (1ULL <<  0)
#define MM_PTE_READWRITE  (1ULL <<  1)
#define MM_PTE_SUPERVISOR (1ULL <<  2)
#define MM_PTE_WRITETHRU  (1ULL <<  3)
#define MM_PTE_CDISABLE   (1ULL <<  4)
#define MM_PTE_ACCESSED   (1ULL <<  5)
#define MM_PTE_DIRTY      (1ULL <<  6)
#define MM_PTE_PAT        (1ULL <<  7)
#define MM_PTE_PAGESIZE   (1ULL <<  7) // in terms of PML3/PML2 entries, for 1GB/2MB pages respectively. Not Used by the Loader
#define MM_PTE_GLOBAL     (1ULL <<  8) // doesn't invalidate the pages from the TLB when CR3 is changed
#define MM_PTE_NOEXEC     (1ULL << 63) // aka eXecute Disable
#define MM_PTE_PKMASK     (15ULL<< 59) // protection key mask. We will not use it.

#define MM_PTE_ADDRESSMASK (0x000FFFFFFFFFF000) // description of the other bits that aren't 1 in the mask:

#define PAGE_SIZE (0x1000)
