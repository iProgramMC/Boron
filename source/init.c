/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	init.c
	
Abstract:
	This module implements the system startup function.
	
Author:
	iProgramInCpp - 28 August 2023
***/

#include <_limine.h>
#include <main.h>
#include <hal.h>
#include <mm.h>
#include <ke.h>

#ifdef TARGET_AMD64
void KiSetupIdt();
#endif

// bootloader requests
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

// The entry point to our kernel.
void KiSystemStartup(void)
{
	HalDebugTerminalInit();
	SLogMsg("Boron is starting up");
	
	HalTerminalInit();
	LogMsg("Boron (TM), September 2023 - V0.002");
	
	// initialize the physical memory manager
	MiInitPMM();
	
	// initialize the IDT if on x86_64
	#ifdef TARGET_AMD64
	KiSetupIdt();
	#endif
	
	// initialize SMP. This function doesn't return
	KeInitSMP();
}

