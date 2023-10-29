/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	arch/amd64/dbg.c
	
Abstract:
	This header file contains the implementation of the
	architecture specific debugging routines.
	
Author:
	iProgramInCpp - 28 October 2023
***/
#include <ke.h>
#include <arch.h>
#include <ldr.h>
#include <hal.h>
#include <string.h>
#include <limreq.h>

#define KERNEL_IMAGE_BASE (0xFFFFFFFF80000000)

// Defined in misc.asm:
uintptr_t KiGetRIP();
uintptr_t KiGetRBP();

// Assume that EBP is the first thing pushed when entering a function. This is often
// the case because we specify -fno-omit-frame-pointer when compiling.
// If not, we are in trouble.
typedef struct STACK_FRAME_tag STACK_FRAME, *PSTACK_FRAME;

struct STACK_FRAME_tag
{
	PSTACK_FRAME Next;
	uintptr_t IP;
};

static void DbgResolveAddress(uintptr_t Address, char *SymbolName, size_t BufferSize)
{
	if (!Address)
	{
		// End of our stack trace
		strcpy(SymbolName, "End");
		return;
	}
	
	strcpy(SymbolName, "??");
	
	uintptr_t BaseAddress = 0;
	// Determine where that address came from.
	if (Address >= KERNEL_IMAGE_BASE)
	{
		// Easy, it's in the kernel.
		// Determine the symbol's name
		const char* Name = DbgLookUpRoutineNameByAddress(Address, &BaseAddress);
		
		if (Name)
		{
			snprintf(SymbolName,
					 BufferSize,
					 "brn!%s+%lx",
					 Name,
					 Address - BaseAddress);
		}
		return;
	}
	
	// Determine which loaded DLL includes this address.
	PLOADED_DLL Dll = NULL;
	
	for (int i = 0; i < KeLoadedDLLCount; i++)
	{
		PLOADED_DLL LoadedDll = &KeLoadedDLLs[i];
		if (LoadedDll->ImageBase <= Address && Address < LoadedDll->ImageBase + LoadedDll->ImageSize)
		{
			// It's the one!
			Dll = LoadedDll;
			break;
		}
	}
	
	if (!Dll)
		return;
	
	Address -= Dll->ImageBase;
	
	const char* Name = LdrLookUpRoutineNameByAddress(Dll, Address, &BaseAddress);
	
	if (Name)
	{
		snprintf(SymbolName,
		         BufferSize,
		         "%s!%s+%lx",
		         Dll->Name,
		         Name,
		         Address - BaseAddress);
	}
}

void DbgPrintStackTrace(uintptr_t Rbp)
{
	if (Rbp == 0)
		Rbp = KiGetRBP();
	
	PSTACK_FRAME StackFrame = (PSTACK_FRAME) Rbp;
	
	int Depth = 30;
	char Buffer[128];
	
	HalDisplayString("\tAddress         \tName\n");
	
	while (StackFrame && Depth > 0)
	{
		uintptr_t Address = StackFrame->IP;
		
		char SymbolName[64];
		DbgResolveAddress(Address, SymbolName, sizeof SymbolName);
		
		snprintf(Buffer, sizeof(Buffer), "\t%p\t%s\n", (void*) Address, SymbolName);
		HalDisplayString(Buffer);
		
		Depth--;
		StackFrame = StackFrame->Next;
	}
	
	if (Depth == 0)
		HalDisplayString("Warning, stack trace too deep, increase the depth in " __FILE__ " if you need it");
}

#ifdef DEBUG

// Progress bar is for debugging and is used to track the amount of steps until the HAL is initialized.

#define DBG_PROGRESS_BAR_HEIGHT (32)

static int DbgpProgressBarStepCount = 1;
static int DbgpProgressBarProgress  = 0;

void DbgFillRectangle(
	struct limine_framebuffer *FrameBuffer,
	int X1,
	int Y1,
	int X2,
	int Y2,
	uint32_t Color)
{
	if (X1 >= X2) return;
	if (Y1 >= Y2) return;
	int Width  = FrameBuffer->width;
	int Height = FrameBuffer->height;
	if (X2 < 0) return;
	if (Y2 < 0) return;
	if (X1 >= Width)  return;
	if (Y1 >= Height) return;
	if (X1 < 0) X1 = 0;
	if (Y1 < 0) Y1 = 0;
	if (X2 >= Width)  X2 = Width;
	if (Y2 >= Height) Y2 = Height;
	
	int Bpp = FrameBuffer->bpp / 8;
	
	switch (Bpp)
	{
		case 4: // 32-bit color
		{
			for (int Y = Y1; Y < Y2; Y++)
			{
				uint32_t* Pixels = (uint32_t*)((uintptr_t)FrameBuffer->address + Y * FrameBuffer->pitch);
				for (int X = X1; X < X2; X++)
					Pixels[X] = Color;
			}
			break;
		}
		case 3: // 24-bit color
		{
			for (int Y = Y1; Y < Y2; Y++)
			{
				uint8_t* Pixels = (uint8_t*)((uintptr_t)FrameBuffer->address + Y * FrameBuffer->pitch);
				for (int X = X1; X < X2; X++)
				{
					Pixels[X * 3 + 0] = (uint8_t)((Color) & 0xFF);
					Pixels[X * 3 + 1] = (uint8_t)((Color >> 8) & 0xFF);
					Pixels[X * 3 + 2] = (uint8_t)((Color >> 16) & 0xFF);
				}
			}
			break;
		}
		case 2: // 16-bit color
		{
			for (int Y = Y1; Y < Y2; Y++)
			{
				uint16_t* Pixels = (uint16_t*)((uintptr_t)FrameBuffer->address + Y * FrameBuffer->pitch);
				for (int X = X1; X < X2; X++)
					Pixels[X] = (uint16_t)Color;
			}
			break;
		}
		default:
			DbgPrint("DbgFillRectangle UNDEFINED");
			break;
	}
}

void DbgSetProgressBarSteps(int Steps)
{
	DbgpProgressBarStepCount = Steps;
	DbgpProgressBarProgress  = 0;
	
	struct limine_framebuffer_response *Response = KeLimineFramebufferRequest.response;
	struct limine_framebuffer *FrameBuffer = Response->framebuffers[0];
	
	// clear it with black?
	DbgFillRectangle(FrameBuffer, 0, 0, FrameBuffer->width, DBG_PROGRESS_BAR_HEIGHT, 0);
}

// iProgramInCpp's stuff. Slows down the boot process by adding an untimed artificial delay
void DbgBusyWaitABit()
{
	for (int i = 0; i < 1000000; i++)
		ASM("pause":::"memory");
}

void DbgAddToProgressBar()
{
	struct limine_framebuffer_response *Response = KeLimineFramebufferRequest.response;
	struct limine_framebuffer *FrameBuffer = Response->framebuffers[0];
	
	int Width = FrameBuffer->width;
	int BlockWidth = Width / DbgpProgressBarStepCount;
	int BlocksPerLine = Width / BlockWidth;
	int BlockHeight = DBG_PROGRESS_BAR_HEIGHT;
	
	int X = DbgpProgressBarProgress % BlocksPerLine;
	int Y = DbgpProgressBarProgress / BlocksPerLine;
	
	DbgFillRectangle(FrameBuffer,
	                 BlockWidth  *  X,
	                 BlockHeight *  Y,
	                 BlockWidth  * (X + 1) - 1, // gives it a segmented look
					 BlockHeight * (Y + 1),
	                 0x0070C0);
	
	DbgpProgressBarProgress++;
	
	DbgBusyWaitABit();
}

#endif
