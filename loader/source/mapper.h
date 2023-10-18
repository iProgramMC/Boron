/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                          mapper.h                           *
\*************************************************************/
#pragma once

#include <loader.h>

typedef void* PPAGEMAP;

PPAGEMAP GetCurrentPageMap();

void InitializeMapper();

void DumpMemMap();

void CommitMemoryMap();

uintptr_t AllocatePages(size_t SizePages, bool IsReservedForKernel);

