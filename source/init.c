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
	LogMsg("Boron (TM), September 2023 - V0.003");
	
	// initialize the physical memory manager
	MiInitPMM();
	
	// phase 1 of HAL initialization on the BSP:
	HalUPInit();
	
	// initialize SMP. This function doesn't return
	KeInitSMP();
}

