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
#include "utils.h"
#include "tests.h"

void PerformMdlTest()
{
	HPAGEMAP PageMap = MmGetCurrentPageMap();
	uintptr_t FixedAddr = 0x40000000;
	size_t FixedSize = 0x10000;
	
	if (!MmMapAnonPages(
		PageMap,
		FixedAddr,
		FixedSize / PAGE_SIZE,
		MM_PTE_READWRITE | MM_PTE_SUPERVISOR,
		false
		))
		KeCrash("Mdl test: Cannot map anon pages!");
	
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
	
	LogMsg("Note: MDL is leaked for now.");
}
