/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	cc/cci.h
	
Abstract:
	This header defines cache manager internal
	function prototypes.
	
Author:
	iProgramInCpp - 4 June 2025
***/
#pragma once

#include <cc.h>
#include <mm.h>

// The internal view cache's size.
#ifdef IS_64_BIT

// 16 MB on 64-bit.
#define VIEW_CACHE_SIZE (16 * 1024 * 1024)

// A maximum of 128 GB (8K * 16M) can be allocated to system space views.
#define VIEW_CACHE_MAX_COUNT (8192)

#else

// 128 KB on 32-bit.
#define VIEW_CACHE_SIZE (128 * 1024)

// A maximum of 128 MB (1K * 128K) can be allocated to system space views.
#define VIEW_CACHE_MAX_COUNT (1024)

#endif

static ALWAYS_INLINE inline
void CcAcquireMutex(PKMUTEX Mutex)
{
	UNUSED BSTATUS Status;
	
	Status = KeWaitForSingleObject(Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	ASSERT(SUCCEEDED(Status));
}
