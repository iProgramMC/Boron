#include "terminal.h"
#include "flanterm.h"
#include "flanterm_backends/fb.h"

#define MARGIN_H 120
#define MARGIN_V 100

#define TERM_TITLE_HEIGHT 23
#define TERM_BACKGROUND_COLOR RGB(85, 85, 85)
#define TERM_BORDER_COLOR RGB(0, 0, 0)
#define TERM_TITLE_COLOR RGB(0, 0, 0)
#define TERM_SHINE_A_COLOR RGB(170, 170, 170)
#define TERM_SHINE_B_COLOR RGB(85, 85, 85)

#define DEFAULT_BG RGB(255, 255, 255)
#define DEFAULT_FG RGB(0, 0, 0)
#define DEFAULT_BG_BRIGHT RGB(255, 255, 255)
#define DEFAULT_FG_BRIGHT RGB(64, 64, 64)

static uint8_t Font[] = {
#include "font.h"
};

struct flanterm_context* FlantermContext;

void DrawFrame(int Left, int Top, int Right, int Bottom)
{
	PGRAPHICS_CONTEXT Ctx = GraphicsContext;
	
	// Fill a rectangle around the terminal window.
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, 0, 0, Ctx->Width, MARGIN_V);
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, 0, Ctx->Height - MARGIN_V, Ctx->Width, MARGIN_V);
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, 0, MARGIN_V, MARGIN_H, Ctx->Height - MARGIN_V * 2);
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, Right, MARGIN_V, MARGIN_H, Ctx->Height - MARGIN_V * 2);
	
	// Draw the window's borders.
	CGDrawRectangle(Ctx, TERM_BORDER_COLOR, Left - 1, Top - 1, Right - Left + 2, Bottom - Top + 2);
	CGDrawRectangle(Ctx, TERM_BORDER_COLOR, Left - 1, Top - 1 - TERM_TITLE_HEIGHT, Right - Left + 2, TERM_TITLE_HEIGHT + 1);
	
	// Draw the shine around the window's title.
	CGFillRectangle(Ctx, TERM_SHINE_A_COLOR, Left, Top - TERM_TITLE_HEIGHT, Right - Left, 1);
	CGFillRectangle(Ctx, TERM_SHINE_A_COLOR, Left, Top - TERM_TITLE_HEIGHT, 1, TERM_TITLE_HEIGHT - 1);
	CGFillRectangle(Ctx, TERM_SHINE_B_COLOR, Left, Top - 2, Right - Left, 1);
	CGFillRectangle(Ctx, TERM_SHINE_B_COLOR, Right - 1, Top - TERM_TITLE_HEIGHT, 1, TERM_TITLE_HEIGHT - 1);
	
	// Fill in the window title's rectangle.
	CGFillRectangle(Ctx, TERM_TITLE_COLOR, Left + 1, Top - TERM_TITLE_HEIGHT + 1, Right - Left - 2, TERM_TITLE_HEIGHT - 3);
}

void FreeThunk(void* Pointer, UNUSED size_t Size)
{
	OSFree(Pointer);
}

BSTATUS SetupTerminal()
{
	PGRAPHICS_CONTEXT Ctx = GraphicsContext;
	int Left, Right, Top, Bottom;
	
	Left = MARGIN_H;
	Right = Ctx->Width - MARGIN_H;
	
	if (Left > Right) {
		Left = 0;
		Right = Ctx->Width;
	}
	
	Top = MARGIN_V;
	Bottom = Ctx->Height - MARGIN_V;
	if (Top > Bottom) {
		Top = 0;
		Bottom = Ctx->Height;
	}
	
	DrawFrame(Left, Top, Right, Bottom);
	
	int RMSz, GMSz, BMSz, RMSh, GMSh, BMSh;
	CGGetColorFormatInfo(GraphicsContext->ColorFormat, &RMSz, &GMSz, &BMSz, &RMSh, &GMSh, &BMSh);
	
	PGRAPHICS_CONTEXT SubContext = CGCreateSubContext(Ctx, Left, Top, Right - Left, Bottom - Top);
	if (!SubContext)
	{
		DbgPrint("ERROR: Could not allocate sub context, out of memory!");
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	uint32_t DefaultBg = CGConvertColorToNative(SubContext, DEFAULT_BG);
	uint32_t DefaultFg = CGConvertColorToNative(SubContext, DEFAULT_FG);
	uint32_t DefaultBgBright = CGConvertColorToNative(SubContext, DEFAULT_BG_BRIGHT);
	uint32_t DefaultFgBright = CGConvertColorToNative(SubContext, DEFAULT_FG_BRIGHT);
	
	FlantermContext = flanterm_fb_init(
		OSAllocate,
		FreeThunk,
		(uint32_t*) SubContext->BufferAddress,
		SubContext->Width,
		SubContext->Height,
		SubContext->Pitch,
		RMSz, RMSh,
		GMSz, GMSh,
		BMSz, BMSh,
		NULL, // Canvas
		NULL, NULL, // AnsiColors, AnsiBrightColors
		&DefaultBg,
		&DefaultFg,
		&DefaultBgBright,
		&DefaultFgBright,
		Font, // Font
		8, 16, 0, // FontWidth, FontHeight, FontSpacing
		1, 1, // FontScaleX, FontScaleY
		0 // Margin
	);
	
	CGFreeContext(SubContext);
	
	if (!FlantermContext)
	{
		DbgPrint("ERROR: flanterm_fb_init failed!");
		
		// Do you know the meme where Windows applications always say "Out of memory!"
		// even if they aren't out of memory, when they fail to do something? Well here
		// is one such case.
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	return STATUS_SUCCESS;
}

void TerminalWrite(const char* Buffer, size_t Length)
{
	flanterm_write(FlantermContext, Buffer, Length);
}
