/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/term.c
	
Abstract:
	This module implements the terminal functions for the i386
	platform.
	
Author:
	iProgramInCpp - 17 October 2025
***/
#include <ke.h>
#include <string.h>
#include "hali.h"
#include "flanterm/src/flanterm.h"
#include "flanterm/src/flanterm_backends/fb.h"

static uint8_t HalpBuiltInFont[] = {
#include "font.h"
};

// NOTE: Initialization done on the BSP. So no need to sync anything
uint8_t* HalpTerminalMemory;
size_t   HalpTerminalMemoryHead;
size_t   HalpTerminalMemorySize;

static struct flanterm_context* HalpTerminalContext;

bool HalIsTerminalInitted()
{
	return HalpTerminalContext != NULL;
}

static void* HalpTerminalMemAlloc(size_t sz)
{
	if (HalpTerminalMemoryHead + sz > HalpTerminalMemorySize)
	{
		DbgPrint("Error, running out of memory in the terminal heap");
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

void HalInitTerminal()
{
	if (KeLoaderParameterBlock.FramebufferCount == 0)
		KeCrashBeforeSMPInit("HAL: No framebuffers found");
	
	PLOADER_FRAMEBUFFER Framebuffer = &KeLoaderParameterBlock.Framebuffers[0];
	uint32_t defaultBG = 0x0000007f;
	uint32_t defaultFG = 0x00ffffff;
	
	// on a 1280x800 screen, the term will have a rez of 160x50 (8000 chars).
	// 52 bytes per character.
	
	const int charWidth = 8, charHeight = 16;
	int charScale = 1;
	
	int termBufWidth  = Framebuffer->Width  / charWidth  / charScale;
	int termBufHeight = Framebuffer->Height / charHeight / charScale;
	
	const int usagePerChar     = 52; // I calculated it
	const int fontBoolMemUsage = charWidth * charHeight * 256; // there are 256 chars
	const int fontDataMemUsage = charWidth * charHeight * 256 / 8;
	const int contextSize      = 256; // seems like sizeof(struct flanterm_context) is no longer exposed, so my best guess
	
	int totalMemUsage = contextSize + fontDataMemUsage + fontBoolMemUsage + termBufWidth * termBufHeight * usagePerChar;
	size_t sizePages = (totalMemUsage + PAGE_SIZE - 1) / PAGE_SIZE;
	
	void* FramebufferMemory = MmMapIoSpace(
		(uintptr_t)Framebuffer->Address,
		Framebuffer->Pitch * Framebuffer->Height,
		MM_PTE_READWRITE | MM_PTE_CDISABLE,
		POOL_TAG("HAFB")
	);
	
	DbgPrint("Screen resolution: %d by %d.  Will use %d Bytes.  Framebuffer mapped at %p.", Framebuffer->Width, Framebuffer->Height, sizePages * PAGE_SIZE, FramebufferMemory);
	
	HalpTerminalMemory       = MmAllocatePoolBig(POOL_FLAG_NON_PAGED, sizePages, POOL_TAG("Term"));
	HalpTerminalMemoryHead   = 0;
	HalpTerminalMemorySize   = sizePages * PAGE_SIZE;
	
	if (!HalpTerminalMemory)
		KeCrashBeforeSMPInit("Error, no memory for the terminal.");
	
	HalpTerminalContext = flanterm_fb_init(
		&HalpTerminalMemAlloc,
		&HalpTerminalFree,
		FramebufferMemory,
		Framebuffer->Width,
		Framebuffer->Height,
		Framebuffer->Pitch,
		Framebuffer->RedMaskSize, Framebuffer->RedMaskShift,     // red mask size and shift
		Framebuffer->GreenMaskSize, Framebuffer->GreenMaskShift, // green mask size and shift
		Framebuffer->BlueMaskSize, Framebuffer->BlueMaskShift,   // blue mask size and shift
		NULL,       // canvas
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

HAL_API void HalDisplayString(const char* Message)
{
	if (!HalpTerminalContext)
	{
		HalPrintStringDebug(Message);
		return;
	}
	
	size_t Length = strlen(Message);
	
	static KSPIN_LOCK SpinLock;
	KIPL Ipl;
	KeAcquireSpinLock(&SpinLock, &Ipl);
	flanterm_write(HalpTerminalContext, Message, Length);
	KeReleaseSpinLock(&SpinLock, Ipl);
}
