// Boron - Hardware abstraction.
// This file provides the functions in the 'terminal' category.
#include <main.h>
#include <string.h>
#include <hal.h>

#ifdef TARGET_AMD64
#include "../arch/amd64/pio.h"
#endif

#include "font.h"

#include <_limine.h>
#include "flanterm/flanterm.h"
#include "flanterm/backends/fb.h"

// NOTE: Initialization done on the BSP. So no need to sync anything
uint8_t g_HalTerminalMemory[512*1024];
int     g_HalTerminalMemoryHead;
#define HAL_TERM_MEM_SIZE (sizeof g_HalTerminalMemory)

// This is defined in init.c
extern volatile struct limine_framebuffer_request g_FramebufferRequest;

struct flanterm_context* g_pTerminalContext;

void* HalTerminalMemAlloc(size_t sz)
{
	if (g_HalTerminalMemoryHead + sz > HAL_TERM_MEM_SIZE)
	{
		SLogMsg("Error, running out of memory in the terminal heap");
		return NULL;
	}
	
	uint8_t* pCurMem = &g_HalTerminalMemory[g_HalTerminalMemoryHead];
	g_HalTerminalMemoryHead += sz;
	return pCurMem;
}

void HalTerminalFree(UNUSED void* pMem, UNUSED size_t sz)
{
}

void HalTerminalInit()
{
	struct limine_framebuffer* pFramebuffer = g_FramebufferRequest.response->framebuffers[0];
	uint32_t defaultBG = 0x0000007f;
	uint32_t defaultFG = 0x00ffffff;
	
	LogMsg("Screen resolution: %d by %d", pFramebuffer->width, pFramebuffer->height);
	
	// on a 1280x800 screen, the term will have a rez of 160x50 (8000 chars).
	// 52 bytes per character.
	
	const int charWidth = 8, charHeight = 16;
	int charScale = 1;
	
	while (true)
	{
		int termBufWidth  = pFramebuffer->width  / charWidth  / charScale;
		int termBufHeight = pFramebuffer->height / charHeight / charScale;
		
		const int usagePerChar     = 52; // I calculated it
		const int fontBoolMemUsage = charWidth * charHeight * 256; // there are 256 chars
		const int fontDataMemUsage = charWidth * charHeight * 256 / 8;
		const int contextSize      = sizeof(struct flanterm_fb_context);
		
		int totalMemUsage = contextSize + fontDataMemUsage + fontBoolMemUsage + termBufWidth * termBufHeight * usagePerChar;
		if (totalMemUsage < (int)HAL_TERM_MEM_SIZE)
			break;
		
		LogMsg("Have to increase due to a lack of reserved terminal memory. TotalMemUsage = %d for charScale %d", totalMemUsage, charScale);
		charScale++;
	}
	
	g_pTerminalContext = flanterm_fb_init(
		&HalTerminalMemAlloc,
		&HalTerminalFree,
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
		g_BuiltInFont, // font pointer
		charWidth,     // font width
		charHeight,    // font height
		0,             // character spacing
		charScale,     // character scale width
		charScale,     // character scale height
		0
	);
	
	if (!g_pTerminalContext)
	{
		SLogMsg("Error, no terminal context");
	}
}

void HalDebugTerminalInit()
{
	// No init required for the 0xE9 hack
}

void HalPrintString(const char* str)
{
	if (!g_pTerminalContext)
	{
		HalPrintStringDebug(str);
		return;
	}
	
	flanterm_write(g_pTerminalContext, str, strlen(str));
}

void HalPrintStringDebug(const char* str)
{
	while (*str)
	{
	#ifdef TARGET_AMD64
		KePortWriteByte(0xE9, *str);
	#else
		#error "May want to implement this for your platform ..."
	#endif
		
		str++;
	}
}
