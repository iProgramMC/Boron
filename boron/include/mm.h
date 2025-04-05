/***
	The Boron Operating System
	Copyright (C) 2023-2025 iProgramInCpp

Module name:
	mm.h
	
Abstract:
	This header file includes every subsystem of the
	Boron Memory Manager.
	
Author:
	iProgramInCpp - 28 August 2023
***/
#ifndef NS64_MM_H
#define NS64_MM_H

#include <main.h>
#include <arch.h>

// Debug flags. Use if something's gone awry
#define MM_DBG_NO_DEMAND_PAGING (0)

#include <mm/pfn.h>
#include <mm/pmm.h>
#include <mm/pool.h>
#include <mm/pt.h>
#include <mm/probe.h>
#include <mm/mdl.h>
#include <mm/cache.h>
#include <mm/section.h>
#include <mm/vad.h>
#include <mm/heap.h>
#include <mm/view.h>
#include <mm/services.h>

#ifdef KERNEL
// Initialize the allocators.
void MmInitAllocators();
#endif

#endif//NS64_MM_H