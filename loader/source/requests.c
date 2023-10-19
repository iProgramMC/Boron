/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                         requests.c                          *
\*************************************************************/
#include "requests.h"
#include <_limine.h>

volatile struct limine_hhdm_request HhdmRequest =
{
	.id = LIMINE_HHDM_REQUEST,
	.revision = 0,
	.response = NULL,
};

volatile struct limine_memmap_request MemMapRequest =
{
	.id = LIMINE_MEMMAP_REQUEST,
	.revision = 0,
	.response = NULL,
};

volatile struct limine_module_request ModuleRequest =
{
	.id = LIMINE_MODULE_REQUEST,
	.revision = 0,
	.response = NULL,
};



uintptr_t GetHhdmOffset()
{
	return HhdmRequest.response->offset;
}

void * GetHhdmOffsetPtr(uintptr_t Addr)
{
	return (void*) (GetHhdmOffset() + Addr);
}
