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

#if 0
#include <limreq.h>

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

const char* KeGetBootCommandLine()
{
	const char* cmdLine = KeLimineKernelFileRequest.response->kernel_file->cmdline;
	if (!cmdLine)
		cmdLine = "";
	
	return cmdLine;
}
#endif
