// 10 November 2025 - iProgramInCpp
#include "terminal.h"

HANDLE KeyboardHandle;

// Table copy pasted from NanoShell.
const unsigned char KeyboardMap[256] =
{
	// shift not pressed.
    0,  27, '1', '2',  '3',  '4', '5', '6',  '7', '8',
  '9', '0', '-', '=', '\x7F', '\t', 'q', 'w',  'e', 'r',
  't', 'y', 'u', 'i',  'o',  'p', '[', ']', '\n',   0,
  'a', 's', 'd', 'f',  'g',  'h', 'j', 'k',  'l', ';',
 '\'', '`',   0,'\\',  'z',  'x', 'c', 'v',  'b', 'n',
  'm', ',', '.', '/',   0,   '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,
	0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	
	// shift pressed.
	0,  0, '!', '@', '#', '$', '%', '^', '&', '*',	/* 9 */
  '(', ')', '_', '+', '\x7F',	/* Backspace */
  '\t',			/* Tab */
  'Q', 'W', 'E', 'R',	/* 19 */
  'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	/* 39 */
 '"', '~',   0,		/* Left shift */
 '|', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
  'M', '<', '>', '?',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,
	0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#define KEY_2             0x03
#define KEY_6             0x07
#define KEY_MINUS         0x0c
#define KEY_BRACKET_LEFT  0x1a
#define KEY_BRACKET_RIGHT 0x1b
#define KEY_BACKSLASH     0x2b
#define KEY_CTRL          0x1d
#define KEY_ALT           0x38
#define KEY_LSHIFT        0x2a
#define KEY_RSHIFT        0x36
#define KEY_ARROW_UP      0x48
#define KEY_ARROW_LEFT    0x4b
#define KEY_ARROW_RIGHT   0x4d
#define KEY_ARROW_DOWN    0x50
#define KEY_F1            0x3b
#define KEY_F2            0x3c
#define KEY_F3            0x3d
#define KEY_F4            0x3e
#define KEY_F5            0x3f
#define KEY_F6            0x40
#define KEY_F7            0x41
#define KEY_F8            0x42
#define KEY_F9            0x43
#define KEY_F10           0x44
#define KEY_F11           0x57
#define KEY_F12           0x58

bool IsShiftPressed = false, IsCtrlPressed = false, IsAltPressed = false;

const char* ArrowKeyModifiers()
{
	static char Buffer[16];
	*Buffer = 0;
	
	if (!IsShiftPressed && !IsCtrlPressed && !IsAltPressed)
		return Buffer;
	
	int Mod = (IsShiftPressed ? 1 : 0) |
	          (IsAltPressed   ? 2 : 0) |
	          (IsCtrlPressed  ? 4 : 0);
	
	snprintf(Buffer, sizeof Buffer, "1;%d", Mod + 1);
	return Buffer;
}

void TranslateKeyCode(char* Input, uint8_t KeyCode)
{
	*Input = 0;
	
	bool IsRelease = KeyCode & 0x80;
	KeyCode &= ~0x80;
	if (KeyCode == KEY_LSHIFT || KeyCode == KEY_RSHIFT) {
		IsShiftPressed = !IsRelease;
		return;
	}
	if (KeyCode == KEY_CTRL) {
		IsCtrlPressed = !IsRelease;
		return;
	}
	if (KeyCode == KEY_ALT) {
		IsAltPressed = !IsRelease;
		return;
	}
	
	if (IsRelease)
		return;
	
	if (IsAltPressed)
		strcpy(Input, "\x1B");
	
	size_t Offset = IsAltPressed;
	switch (KeyCode)
	{
		case KEY_ARROW_UP:    snprintf(Input + Offset, 16 - Offset, "\x1B[%sA", ArrowKeyModifiers()); return;
		case KEY_ARROW_DOWN:  snprintf(Input + Offset, 16 - Offset, "\x1B[%sB", ArrowKeyModifiers()); return;
		case KEY_ARROW_RIGHT: snprintf(Input + Offset, 16 - Offset, "\x1B[%sC", ArrowKeyModifiers()); return;
		case KEY_ARROW_LEFT:  snprintf(Input + Offset, 16 - Offset, "\x1B[%sD", ArrowKeyModifiers()); return;
	}
	
	if (KeyCode >= KEY_F1 && KeyCode <= KEY_F10) {
		snprintf(Input + Offset, 16 - Offset, "\x1B[%d~", KeyCode + 11 - KEY_F1);
		return;
	}
	
	if (KeyCode >= KEY_F11 && KeyCode <= KEY_F12) {
		snprintf(Input + Offset, 16 - Offset, "\x1B[%d~", KeyCode + 23 - KEY_F11);
		return;
	}
	
	if (IsCtrlPressed)
	{
		// ^A == 0x01, ^Z == 0x1A
		char Ascii = KeyboardMap[KeyCode];
		char Typed = Ascii;
		if (Ascii >= 'a' || Ascii <= 'z')
		{
			Typed = 1 + (Ascii - 'a');
		}
		else switch (Ascii)
		{
			case KEY_BRACKET_LEFT:   Typed = '\x1B'; break;
			case KEY_BACKSLASH:      Typed = '\x1C'; break;
			case KEY_BRACKET_RIGHT:  Typed = '\x1D'; break;
			case KEY_2:              if (IsShiftPressed) Typed = '\x00'; break; // note: this won't work
			case KEY_6:              if (IsShiftPressed) Typed = '\x1E'; break;
			case KEY_MINUS:          if (IsShiftPressed) Typed = '\x1F'; break;
		}
		
		Input[Offset++] = Typed;
	}
	else
	{
		Input[Offset++] = KeyboardMap[IsShiftPressed * 128 + KeyCode];
	}
	
	Input[Offset++] = 0;
}

BSTATUS UseKeyboard(const char* KeyboardName)
{
	BSTATUS Status;
	OBJECT_ATTRIBUTES Attributes;
	Attributes.ObjectName = KeyboardName;
	Attributes.ObjectNameLength = strlen(KeyboardName);
	Attributes.OpenFlags = 0;
	Attributes.RootDirectory = HANDLE_NONE;
	
	Status = OSOpenFile(&KeyboardHandle, &Attributes);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Couldn't open keyboard '%s'. %s (%d)", KeyboardName, RtlGetStatusString(Status), Status);
		return Status;
	}
	
	return Status;
}
