/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mdltst.c
	
Abstract:
	This module implements the MDL test for the test driver.
	It tests the functionality of the MDL.
	
Author:
	iProgramInCpp - 8 February 2024
***/
#include <mm.h>
#include <string.h>
#include "utils.h"
#include "tests.h"

void PerformMdlTest()
{
	// allow it to run to prevent the initialization thread from interfering with our page count
	
	/*
	LogMsg("Wait 2 sec!");
	KTIMER Timer;
	KeInitializeTimer(&Timer);
	KeSetTimer(&Timer, 2000, NULL);
	KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE);
	*/
	
	HPAGEMAP PageMap = MiGetCurrentPageMap();
	uintptr_t FixedAddr = 0x40000000;
	size_t FixedSize = 0x10000;
	
	if (!MiMapAnonPages(
		PageMap,
		FixedAddr,
		FixedSize / PAGE_SIZE,
		MM_PTE_READWRITE | MM_PTE_SUPERVISOR,
		false
		))
		KeCrash("Mdl test: Cannot map anon pages!");
	
	memset((void*) FixedAddr, 0x5A, FixedSize);
	
	LogMsg("Memory Pages Available Now: %zu", MmGetTotalFreePages());
	
	// mapped, now create the MDL
	PMDL Mdl = MmAllocateMdl(FixedAddr, FixedSize);
	
	if (!Mdl)
		KeCrash("Mdl test: could not allocate MDL");
	
	BSTATUS Status = MmProbeAndPinPagesMdl(Mdl);
	
	if (Status != STATUS_SUCCESS)
		KeCrash("Mdl test: CaptureMDL returned %d", Status);
	
	// Dump all info about it:
	LogMsg("MDL captured! Here's some info about it:");
	LogMsg("	ByteOffset:    %d",   Mdl->ByteOffset);
	LogMsg("	Flags:         %04x", Mdl->Flags);
	LogMsg("	ByteCount:     %zu",  Mdl->ByteCount);
	LogMsg("	MappedStartVA: %p",   Mdl->MappedStartVA);
	LogMsg("	Process:       %p",   Mdl->Process);
	LogMsg("	NumberPages:   %zu",  Mdl->NumberPages);
	
	LogMsg("[Printing pages to debug]");
	for (size_t i = 0; i < Mdl->NumberPages; i++)
	{
		int Pfn = Mdl->Pages[i];
		DbgPrint("* Pages[%d] = %d (%p)", i, Pfn, (uintptr_t)Pfn << 12);
	}
	
	LogMsg("Mapping it into system memory now.");
	
	uintptr_t MapAddress = 0;
	Status = MmMapPinnedPagesMdl(Mdl, &MapAddress, MM_PTE_READWRITE | MM_PTE_SUPERVISOR);
	
	if (Status != STATUS_SUCCESS)
		KeCrash("Mdl test: MapMDL returned %d", Status);
	
	// Try accessing that address.
	LogMsg("Addr: %p", MapAddress);
	*((uint32_t*) MapAddress) = 0x5A5A5A5A;
	
	LogMsg("Read in from fixedaddr: %08x", *((uint32_t*) FixedAddr));
	
	MmFreeMdl(Mdl);
	
	LogMsg("Memory Pages Available Now: %zu", MmGetTotalFreePages());
	
	MiUnmapPages(PageMap, FixedAddr, FixedSize);
	
	// note: the variation of 3 pages is actually normal at this point
	// the pages are allocated during the initial mapping
	
	// Try again!
	//LogMsg("Trying again:");
	//*((uint32_t*)MapAddress) = 0x42424242;
}
