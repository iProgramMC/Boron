#include "terminal.h"
#include "flanterm/src/flanterm.h"
#include "flanterm_alt_fb.h"

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

PGRAPHICS_CONTEXT GraphicsContext;
struct flanterm_context* FlantermContext;

int MarginVert = 100;
int MarginHorz = 120;

bool Frameless = false;

void DrawFrame(int Left, int Top, int Right, int Bottom)
{
	PGRAPHICS_CONTEXT Ctx = GraphicsContext;
	
	// Fill a rectangle around the terminal window.
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, 0, 0, Ctx->Width, MarginVert);
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, 0, Ctx->Height - MarginVert, Ctx->Width, MarginVert);
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, 0, MarginVert, MarginHorz, Ctx->Height - MarginVert * 2);
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, Right, MarginVert, MarginHorz, Ctx->Height - MarginVert * 2);
	
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

void TerminalWrite(const char* Buffer, size_t Length)
{
	flanterm_write(FlantermContext, Buffer, Length);
}

BSTATUS SetupTerminal()
{
	PGRAPHICS_CONTEXT Ctx = GraphicsContext;
	int Left, Right, Top, Bottom;
	
	if (Frameless)
	{
		Left = Top = 0;
		Right = Ctx->Width;
		Bottom = Ctx->Height;
	}
	else
	{
		Left = MarginHorz;
		Right = Ctx->Width - MarginHorz;
		if (Left > Right) {
			Left = 0;
			Right = Ctx->Width;
		}
		
		Top = MarginVert;
		Bottom = Ctx->Height - MarginVert;
		if (Top > Bottom) {
			Top = 0;
			Bottom = Ctx->Height;
		}
		
		DrawFrame(Left, Top, Right, Bottom);
	}
	
	int RMSz, GMSz, BMSz, RMSh, GMSh, BMSh;
	CGGetColorFormatInfo(GraphicsContext->ColorFormat, &RMSz, &GMSz, &BMSz, &RMSh, &GMSh, &BMSh);
	
	PGRAPHICS_CONTEXT SubContext = CGCreateSubContext(Ctx, Left, Top, Right - Left, Bottom - Top);
	if (!SubContext)
	{
		DbgPrint("ERROR: Could not allocate sub context, out of memory!");
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	uint32_t DefaultBg = DEFAULT_BG;
	uint32_t DefaultFg = DEFAULT_FG;
	uint32_t DefaultBgBright = DEFAULT_BG_BRIGHT;
	uint32_t DefaultFgBright = DEFAULT_FG_BRIGHT;
	
	FlantermContext = flanterm_fb_init_alt(
		OSAllocate,
		FreeThunk,
		(uint32_t*) SubContext->BufferAddress,
		SubContext->Width,
		SubContext->Height,
		SubContext->Pitch,
		SubContext->Bpp,
		RMSz, RMSh,
		GMSz, GMSh,
		BMSz, BMSh,
		NULL, NULL, // AnsiColors, AnsiBrightColors
		&DefaultBg,
		&DefaultFg,
		&DefaultBgBright,
		&DefaultFgBright,
		Font, // Font
		8, 16, 0, // FontWidth, FontHeight, FontSpacing
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

BSTATUS UseFramebuffer(const char* FramebufferPath)
{
	BSTATUS Status;
	OBJECT_ATTRIBUTES Attributes;
	Attributes.ObjectName = FramebufferPath;
	Attributes.ObjectNameLength = strlen(FramebufferPath);
	Attributes.OpenFlags = OB_OPEN_OBJECT_NAMESPACE;
	Attributes.RootDirectory = HANDLE_NONE;
	
	HANDLE FramebufferHandle;
	Status = OSOpenFile(&FramebufferHandle, &Attributes);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Couldn't open framebuffer '%s'. %s (%d)", FramebufferPath, RtlGetStatusString(Status), Status);
		return Status;
	}
	
	IOCTL_FRAMEBUFFER_INFO FbInfo;
	Status = OSDeviceIoControl(
		FramebufferHandle,
		IOCTL_FRAMEBUFFER_GET_INFO,
		NULL,
		0,
		&FbInfo,
		sizeof FbInfo
	);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Failed to get framebuffer info from %s. %s (%d)", FramebufferPath, RtlGetStatusString(Status), Status);
		OSClose(FramebufferHandle);
		return Status;
	}
	
	void* FbAddress = NULL;
	size_t Size = FbInfo.Pitch * FbInfo.Height;
	Status = OSMapViewOfObject(
		CURRENT_PROCESS_HANDLE,
		FramebufferHandle,
		&FbAddress,
		Size,
		MEM_COMMIT,
		0,
		PAGE_READ | PAGE_WRITE
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Size: %zu", Size);
		DbgPrint("ERROR: Failed to map %s. %s (%d)", FramebufferPath, RtlGetStatusString(Status), Status);
		OSClose(FramebufferHandle);
		return Status;
	}
	
	if (FbInfo.Width < 800)
		MarginHorz /= 2;
	if (FbInfo.Height < 600)
		MarginVert /= 2;
	
	if (FbInfo.Width < 700 || FbInfo.Height < 500) {
		Frameless = true;
	}

	GraphicsContext = CGCreateContextFromBuffer(
		FbAddress,
		(int) FbInfo.Width,
		(int) FbInfo.Height,
		FbInfo.Pitch,
		FbInfo.Bpp,
		FbInfo.ColorFormat
	);
	
	if (!GraphicsContext)
	{
		DbgPrint("ERROR: Failed to create graphics context %s.", FramebufferPath);
		OSClose(FramebufferHandle);
		return Status;
	}
	
	// don't need the handle to the file anymore, since we mapped it
	OSClose(FramebufferHandle);
	return STATUS_SUCCESS;
}

void TerminalGetDimensions(int* Width, int* Height)
{
	size_t Cols = 1, Rows = 1;
	flanterm_get_dimensions(FlantermContext, &Cols, &Rows);
	*Width = (int) Cols;
	*Height = (int) Rows;
}
