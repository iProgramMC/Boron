/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	limreq.h
	
Abstract:
	This header file contains forward declarations for all Limine
	bootloader requests.
	
Author:
	iProgramInCpp - 28 August 2023
***/
#ifndef BORON_LIMREQ_H
#define BORON_LIMREQ_H

#include <_limine.h>

// Request IDs, for use in the HAL:
enum
{
	KLGR_NONE = 0,
	KLGR_HHDM,
	KLGR_FRAMEBUFFER,
	KLGR_MEMMAP,
	KLGR_MODULE,
	KLGR_RSDP,
	KLGR_SMP,
	KLGR_KERNELFILE,
	KLGR_COUNT,
};

#ifdef KERNEL

extern volatile struct limine_hhdm_request KeLimineHhdmRequest;
extern volatile struct limine_framebuffer_request KeLimineFramebufferRequest;
extern volatile struct limine_memmap_request KeLimineMemMapRequest;
extern volatile struct limine_smp_request KeLimineSmpRequest;
extern volatile struct limine_rsdp_request KeLimineRsdpRequest;
extern volatile struct limine_module_request KeLimineModuleRequest;
extern volatile struct limine_kernel_file_request KeLimineKernelFileRequest;

#else

volatile void* KeLimineGetRequest(int RequestId);

#define KeLimineHhdmRequest        (*(volatile struct limine_hhdm_request       *)KeLimineGetRequest(KLGR_HHDM))
#define KeLimineFramebufferRequest (*(volatile struct limine_framebuffer_request*)KeLimineGetRequest(KLGR_FRAMEBUFFER))
#define KeLimineMemMapRequest      (*(volatile struct limine_memmap_request     *)KeLimineGetRequest(KLGR_MEMMAP))
#define KeLimineSmpRequest         (*(volatile struct limine_smp_request        *)KeLimineGetRequest(KLGR_SMP))
#define KeLimineRsdpRequest        (*(volatile struct limine_rsdp_request       *)KeLimineGetRequest(KLGR_RSDP))
#define KeLimineModuleRequest      (*(volatile struct limine_module_request     *)KeLimineGetRequest(KLGR_MODULE))
#define KeLimineKernelFileRequest  (*(volatile struct limine_kernel_file_request*)KeLimineGetRequest(KLGR_KERNELFILE))

#endif

#endif//BORON_LIMREQ_H