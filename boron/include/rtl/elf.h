/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	rtl/elf.h
	
Abstract:
	This header file defines function prototypes for
	ELF loading helpers.
	
Author:
	iProgramInCpp - 30 April 2024
***/
#pragma once

#include <elf.h>

bool RtlPerformRelocations(PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase);

bool RtlParseDynamicTable(PELF_DYNAMIC_ITEM DynItem, PELF_DYNAMIC_INFO Info, uintptr_t LoadBase);

bool RtlLinkPlt(PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase, UNUSED const char* FileName);
