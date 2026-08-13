/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	io/ccinfo.c
	
Abstract:
	This module implements functions regarding the I/O
	Cache Info structure.
	
Author:
	iProgramInCpp - 12 August 2026
***/
#include <io.h>
#include <ke.h>
#include <mm.h>

void IoInitializeCacheInfo(PIO_CACHE_INFO CacheInfo)
{
	MmInitializeCcb(&CacheInfo->PageCache);
	KeInitializeMutex(&CacheInfo->ViewCacheMutex, 0);
}

void IoTeardownCacheInfo(PIO_CACHE_INFO CacheInfo)
{
	MmTearDownCcb(&CacheInfo->PageCache);
}
