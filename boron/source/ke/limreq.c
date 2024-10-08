/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/limreq.c
	
Abstract:
	This module contains the list of Limine bootloader requests.
	
Author:
	iProgramInCpp - 23 September 2023
***/
#include <stddef.h>
#include <_limine.h>
#include <limreq.h>

volatile struct limine_hhdm_request KeLimineHhdmRequest =
{
	.id = LIMINE_HHDM_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_framebuffer_request KeLimineFramebufferRequest =
{
	.id = LIMINE_FRAMEBUFFER_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_memmap_request KeLimineMemMapRequest =
{
	.id       = LIMINE_MEMMAP_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_module_request KeLimineModuleRequest =
{
	.id       = LIMINE_MODULE_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_smp_request KeLimineSmpRequest =
{
	.id = LIMINE_SMP_REQUEST,
	.revision = 0,
	.response = NULL,
	.flags = 0,
};
volatile struct limine_rsdp_request KeLimineRsdpRequest =
{
	.id       = LIMINE_RSDP_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_kernel_file_request KeLimineKernelFileRequest =
{
	.id       = LIMINE_KERNEL_FILE_REQUEST,
	.revision = 0,
	.response = NULL,
};

// NOTE: Requesting the kernel file for two reasons:
// - 1. It's a possibility that I'll be phasing out the existing symbol table system and
// - 2. It seems like Limine unconditionally occupies bootloader reclaimable memory with the kernel file.

// Note: This MUST match the order of the KLGR enum.
static volatile void* const KepLimineRequestTable[] =
{
	NULL,
	&KeLimineHhdmRequest,
	&KeLimineFramebufferRequest,
	&KeLimineMemMapRequest,
	&KeLimineSmpRequest,
	&KeLimineRsdpRequest,
	&KeLimineModuleRequest,
	&KeLimineKernelFileRequest,
};

volatile void* KeLimineGetRequest(int RequestId)
{
	if (RequestId <= KLGR_NONE || RequestId >= KLGR_COUNT)
		return NULL;
	
	return KepLimineRequestTable[RequestId];
}
