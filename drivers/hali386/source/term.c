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
#include "flanterm_alt_fb.h"

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

bool HalCheckForVideoBootArgument();

void HalInitTerminal()
{
	if (KeLoaderParameterBlock.FramebufferCount == 0)
	{
		DbgPrint("HAL: No framebuffers provided through multiboot.");
		
		// grub4dos actually provides another way.  It appends a string parameter called "video".
		// Example: "video=1024x768x16@fd000000,2048"
		if (!HalCheckForVideoBootArgument())
			KeCrashBeforeSMPInit("HAL: No framebuffers found");
	}
	
	PLOADER_FRAMEBUFFER Framebuffer = &KeLoaderParameterBlock.Framebuffers[0];
	uint32_t defaultBG = 0x0000007f;
	uint32_t defaultFG = 0x00ffffff;
	
	// on a 1280x800 screen, the term will have a rez of 160x50 (8000 chars).
	// 52 bytes per character.
	
	const int charWidth = 8, charHeight = 16;
	int termBufWidth  = Framebuffer->Width  / charWidth;
	int termBufHeight = Framebuffer->Height / charHeight;
	
	const int usagePerChar     = 52; // I calculated it
	const int fontBoolMemUsage = charWidth * charHeight * 256; // there are 256 chars
	const int fontDataMemUsage = charWidth * charHeight * 256 / 8;
	const int contextSize      = 256; // seems like sizeof(struct flanterm_context) is no longer exposed, so my best guess
	
	int totalMemUsage = contextSize + fontDataMemUsage + fontBoolMemUsage + termBufWidth * termBufHeight * usagePerChar;
	size_t sizePages = (totalMemUsage + PAGE_SIZE - 1) / PAGE_SIZE;
	
	void* FramebufferMemory = MmMapIoSpace(
		(uintptr_t)Framebuffer->Address,
		Framebuffer->Pitch * Framebuffer->Height,
		MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
		POOL_TAG("HAFB")
	);
	
	DbgPrint("Screen resolution: %d by %d.  Will use %d Bytes.  Framebuffer mapped at %p.", Framebuffer->Width, Framebuffer->Height, sizePages * PAGE_SIZE, FramebufferMemory);
	
	HalpTerminalMemory       = MmAllocatePoolBig(POOL_FLAG_NON_PAGED, sizePages, POOL_TAG("Term"));
	HalpTerminalMemoryHead   = 0;
	HalpTerminalMemorySize   = sizePages * PAGE_SIZE;
	
	if (!HalpTerminalMemory)
		KeCrashBeforeSMPInit("Error, no memory for the terminal.");
	
	HalpTerminalContext = flanterm_fb_init_alt(
		&HalpTerminalMemAlloc,
		&HalpTerminalFree,
		FramebufferMemory,
		Framebuffer->Width,
		Framebuffer->Height,
		Framebuffer->Pitch,
		Framebuffer->BitDepth,
		Framebuffer->RedMaskSize, Framebuffer->RedMaskShift,     // red mask size and shift
		Framebuffer->GreenMaskSize, Framebuffer->GreenMaskShift, // green mask size and shift
		Framebuffer->BlueMaskSize, Framebuffer->BlueMaskShift,   // blue mask size and shift
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

bool HalCheckForVideoBootArgument()
{
	// We have a problem: the boot configuration isn't actually initialized yet.
	// Thus we'll need to fish it directly out of KeLoaderParameterBlock.
	//
	// Format: "video=1024x768x16@fd000000,2048"
	const char* CommandLine = KeLoaderParameterBlock.CommandLine;
	while (*CommandLine && memcmp(CommandLine, "video=", 6) != 0) {
		CommandLine++;
	}
	
	if (!*CommandLine) {
		DbgPrint("not found!");
		return false;
	}
	
	int Width = 0, Height = 0, Bpp = 0, Pitch = 0;
	uintptr_t Address = 0;
	
	CommandLine += 6;
	
	// Read Width
	while (*CommandLine >= '0' && *CommandLine <= '9') {
		Width = Width * 10 + (*CommandLine - '0');
		CommandLine++;
	}
	
	if (*CommandLine++ != 'x') return false;
	
	// Read Height
	while (*CommandLine >= '0' && *CommandLine <= '9') {
		Height = Height * 10 + (*CommandLine - '0');
		CommandLine++;
	}
	
	if (*CommandLine++ != 'x') return false;
	
	// Read Bpp
	while (*CommandLine >= '0' && *CommandLine <= '9') {
		Bpp = Bpp * 10 + (*CommandLine - '0');
		CommandLine++;
	}
	
	if (*CommandLine++ != '@') return false;
	if (*CommandLine++ != '0') return false;
	if (*CommandLine++ != 'x') return false;
	
	// Read Frame Buffer Address
	while ((*CommandLine >= '0' && *CommandLine <= '9')
		|| (*CommandLine >= 'A' && *CommandLine <= 'F')
		|| (*CommandLine >= 'a' && *CommandLine <= 'f'))
	{
		Address <<= 4;
		/**/ if (*CommandLine >= 'A' && *CommandLine <= 'F') Address += 10 + (*CommandLine - 'A');
		else if (*CommandLine >= 'a' && *CommandLine <= 'f') Address += 10 + (*CommandLine - 'a');
		else Address += (*CommandLine - '0');
		CommandLine++;
	}
	
	if (*CommandLine++ != ',') return false;
	
	// Read Pitch
	while (*CommandLine >= '0' && *CommandLine <= '9') {
		Pitch = Pitch * 10 + (*CommandLine - '0');
		CommandLine++;
	}
	
	// Basic Sanitization
	if (Width == 0) {
		DbgPrint("No width!");
		return false;
	}
	
	if (Height == 0) {
		DbgPrint("No height!");
		return false;
	}
	
	if (Bpp != 16 && Bpp != 24 && Bpp != 32) {
		DbgPrint("Bad bpp of %d!", Bpp);
		return false;
	}
	
	if (Pitch < Width * Bpp / 8) {
		DbgPrint("Bad pitch of %d, with width of %d!", Pitch, Width);
		return false;
	}
	
	if ((Address & PAGE_SIZE) != 0) {
		DbgPrint("Bad address of %p!", Address);
		return false;
	}
	
	// Okay, register the frame buffer into the LPB now.
	// We can do this because the HAL is the first driver initialized.
	
	DbgPrint("Using command-line provided framebuffer parameters.");
	DbgPrint("width: %d height: %d bpp: %d pitch: %d address: %p", Width,Height,Bpp,Pitch,Address);
	
	static LOADER_FRAMEBUFFER SaveFb;
	SaveFb.Width = Width;
	SaveFb.Height = Height;
	SaveFb.Pitch = Pitch;
	SaveFb.BitDepth = Bpp;
	SaveFb.Address = (void*) Address;
	
	// Assume Hardcoded Properties
	switch (Bpp)
	{
		case 16:
			SaveFb.RedMaskSize     = 5;
			SaveFb.RedMaskShift    = 11;
			SaveFb.GreenMaskSize   = 6;
			SaveFb.GreenMaskShift  = 5;
			SaveFb.BlueMaskSize    = 5;
			SaveFb.BlueMaskShift   = 0;
			break;
		case 24:
		case 32:
			SaveFb.RedMaskSize     = 8;
			SaveFb.RedMaskShift    = 16;
			SaveFb.GreenMaskSize   = 8;
			SaveFb.GreenMaskShift  = 8;
			SaveFb.BlueMaskSize    = 8;
			SaveFb.BlueMaskShift   = 0;
			break;
	}
	
	KeLoaderParameterBlock.Framebuffers = &SaveFb;
	KeLoaderParameterBlock.FramebufferCount = 1;
	return true;
}
