/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                         requests.h                          *
\*************************************************************/
#pragma once
#include <loader.h>
#include <_limine.h>

extern volatile struct limine_hhdm_request   HhdmRequest;
extern volatile struct limine_memmap_request MemMapRequest;
extern volatile struct limine_module_request ModuleRequest;

uintptr_t GetHhdmOffset();

void* GetHhdmOffsetPtr(uintptr_t Addr);

