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

extern volatile struct limine_hhdm_request KeLimineHhdmRequest;
extern volatile struct limine_framebuffer_request KeLimineFramebufferRequest;
extern volatile struct limine_memmap_request KeLimineMemMapRequest;
extern volatile struct limine_smp_request KeLimineSmpRequest;
extern volatile struct limine_rsdp_request KeLimineRsdpRequest;

#endif//BORON_LIMREQ_H