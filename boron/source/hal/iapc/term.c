/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/term.c
	
Abstract:
	This module implements the terminal functions for the AMD64
	platform.
	
Author:
	iProgramInCpp - 20 August 2023
***/

#include <main.h>
#include <string.h>
#include <hal.h>
#include <mm.h>
#include <ke.h>
#include "../../arch/amd64/pio.h"
#include "font.h"
#include <_limine.h>
#include "flanterm/flanterm.h"
#include "flanterm/backends/fb.h"

// NOTE: Initialization done on the BSP. So no need to sync anything
uint8_t* HalpTerminalMemory;
size_t   HalpTerminalMemoryHead;
size_t   HalpTerminalMemorySize;

// This is defined in init.c
extern volatile struct limine_framebuffer_request KeLimineFramebufferRequest;

static struct flanterm_context* HalpTerminalContext;

bool HalIsTerminalInitted()
{
	return HalpTerminalContext != NULL;
}

static void* HalpTerminalMemAlloc(size_t sz)
{
	if (HalpTerminalMemoryHead + sz > HalpTerminalMemorySize)
	{
		SLogMsg("Error, running out of memory in the terminal heap");
		return NULL;
	}
	
	uint8_t* pCurMem = &HalpTerminalMemory[HalpTerminalMemoryHead];
	HalpTerminalMemoryHead += sz;
	return pCurMem;
}

static void HalpTerminalFree(UNUSED void* pMem, UNUSED size_t sz)
{
}

bool HalpIsSerialAvailable;

uintptr_t MiAllocateMemoryFromMemMap(size_t SizeInPages);

void HalTerminalInit()
{
	struct limine_framebuffer* pFramebuffer = KeLimineFramebufferRequest.response->framebuffers[0];
	uint32_t defaultBG = 0x0000007f;
	uint32_t defaultFG = 0x00ffffff;
	
	// on a 1280x800 screen, the term will have a rez of 160x50 (8000 chars).
	// 52 bytes per character.
	
	const int charWidth = 8, charHeight = 16;
	int charScale = 1;
	
	int termBufWidth  = pFramebuffer->width  / charWidth  / charScale;
	int termBufHeight = pFramebuffer->height / charHeight / charScale;
	
	const int usagePerChar     = 52; // I calculated it
	const int fontBoolMemUsage = charWidth * charHeight * 256; // there are 256 chars
	const int fontDataMemUsage = charWidth * charHeight * 256 / 8;
	const int contextSize      = sizeof(struct flanterm_fb_context);
	
	int totalMemUsage = contextSize + fontDataMemUsage + fontBoolMemUsage + termBufWidth * termBufHeight * usagePerChar;
	size_t sizePages = (totalMemUsage + PAGE_SIZE - 1) / PAGE_SIZE;
	
	LogMsg("Screen resolution: %d by %d.  Will use %d Bytes", pFramebuffer->width, pFramebuffer->height, sizePages * PAGE_SIZE);
	
	HalpTerminalMemory     = MmGetHHDMOffsetAddr(MiAllocateMemoryFromMemMap(sizePages));
	HalpTerminalMemoryHead = 0;
	HalpTerminalMemorySize = sizePages * PAGE_SIZE;
	
	HalpTerminalContext = flanterm_fb_init(
		&HalpTerminalMemAlloc,
		&HalpTerminalFree,
		(uint32_t*) pFramebuffer->address,
		pFramebuffer->width,
		pFramebuffer->height,
		pFramebuffer->pitch,
		// no canvas
		NULL,       // ansi colors
		NULL,       // ansi bright colors
		&defaultBG, // default background
		&defaultFG, // default foreground
		NULL,       // default background bright
		NULL,       // default fontground bright
		HalpBuiltInFont, // font pointer
		charWidth,     // font width
		charHeight,    // font height
		0,             // character spacing X
		charScale,     // character scale width
		charScale,     // character scale height
		0              // character spacing Y
	);
	
	if (!HalpTerminalContext)
	{
		KeCrashBeforeSMPInit("Error, no terminal context");
	}
}

void HalPrintString(const char* str)
{
	if (!HalpTerminalContext)
	{
		HalPrintStringDebug(str);
		return;
	}
	
	flanterm_write(HalpTerminalContext, str, strlen(str));
}
