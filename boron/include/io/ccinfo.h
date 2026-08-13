/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	io/ccinfo.h
	
Abstract:
	This header defines the I/O Cache Info structure.
	Not to be confused with the Cache Control Block
	structure from the memory manager.  This structure
	contains the View Cache tree, the Page Cache tree,
	as well as controlling when the Lazy Writer kicks in.
	
Author:
	iProgramInCpp - 12 August 2026
***/
#pragma once

#include <ex/rwlock.h>
#include <mm/cache.h>
#include <io/dispatch.h>

typedef struct
{
	KMUTEX ViewCacheMutex;
	RBTREE ViewCache;
	
	CCB PageCache;
}
IO_CACHE_INFO, *PIO_CACHE_INFO;

void IoInitializeCacheInfo(PIO_CACHE_INFO CacheInfo);

void IoTeardownCacheInfo(PIO_CACHE_INFO CacheInfo);
