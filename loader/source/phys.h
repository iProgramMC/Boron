/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                           phys.h                            *
\*************************************************************/
#pragma once

uintptr_t AllocatePages(size_t SizePages, bool IsReservedForKernel);

void InitializePmm();
