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
