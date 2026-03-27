// 10 November 2025 - iProgramInCpp
#include "terminal.h"

HANDLE KeyboardHandle;

static const uint8_t KeyboardMapNoShift[] = {
	27,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // F1-F12
	0, 0, 0, // PrtScr, ScrollLock, PauseBreak
	'`', '-', '=', '\x7F',
	0, // NumLock
	'/', '*', '-', '+', '.', '\n',
	'\t',
	'[', ']', '\\',
	0, // Caps Lock
	';', '\'', '\n',
	',', '.', '/', ' ',
	0, 0, 0, 0, 0, 0, 0, 0, 0, // Modifiers and Menu
	0, 0, 0, 0, // Arrow Keys
	0, 0, 0, 0, 0, 0, // Ins, Del, Home, End, PgUp, PgDn
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
	'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
	'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
};

static const uint8_t KeyboardMapShift[] = {
	27,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // F1-F12
	0, 0, 0, // PrtScr, ScrollLock, PauseBreak
	'~', '_', '+', '\x7F',
	0, // NumLock
	'/', '*', '-', '+', '.', '\n',
	'\t',
	'{', '}', '|',
	0, // Caps Lock
	':', '"', '\n',
	'<', '>', '?', ' ',
	0, 0, 0, 0, 0, 0, 0, 0, 0, // Modifiers and Menu
	0, 0, 0, 0, // Arrow Keys
	0, 0, 0, 0, 0, 0, // Ins, Del, Home, End, PgUp, PgDn
	')', '!', '@', '#', '$', '%', '^', '&', '*', '(',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
	'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
	'U', 'V', 'W', 'X', 'Y', 'Z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
};

static const int FunctionKeyLookUp[] = {
	15, 17, 18, 19, 20, 21, 23, 24 // F5-F12
};

bool IsShiftPressed = false, IsCtrlPressed = false, IsAltPressed = false;

int CalculateModifierFlags()
{
	return ((IsShiftPressed ? 1 : 0) |
	        (IsAltPressed   ? 2 : 0) |
	        (IsCtrlPressed  ? 4 : 0)) + 1;
}

const char* ModifierBuffer(const char* Format, const char* Default)
{
	static char Buffer[16];
	if (!IsShiftPressed && !IsCtrlPressed && !IsAltPressed)
		return Default;
	
	snprintf(Buffer, sizeof Buffer, Format, CalculateModifierFlags());
	return Buffer;
}

const char* ArrowKeyModifiers()
{
	return ModifierBuffer("1;%d", "");
}

const char* F1F4KeyModifiers()
{
	return ModifierBuffer("1;%d", "O");
}

const char* F5F12KeyModifiers()
{
	return ModifierBuffer(";%d", "");
}

void TranslateKeyCode(char* Input, uint16_t KeyCode)
{
	*Input = 0;
	
	bool IsRelease = KeyCode & KBDEV_KEY_RELEASE;
	KeyCode &= ~KBDEV_KEY_RELEASE;
	
	if (KeyCode == KBDEV_KEY_LEFT_SHIFT || KeyCode == KBDEV_KEY_RIGHT_SHIFT) {
		IsShiftPressed = !IsRelease;
		return;
	}
	if (KeyCode == KBDEV_KEY_LEFT_CONTROL || KeyCode == KBDEV_KEY_RIGHT_CONTROL) {
		IsCtrlPressed = !IsRelease;
		return;
	}
	if (KeyCode == KBDEV_KEY_LEFT_ALT || KeyCode == KBDEV_KEY_RIGHT_ALT) {
		IsAltPressed = !IsRelease;
		return;
	}
	
	if (IsRelease)
		return;
	
	size_t Offset = IsAltPressed;
	switch (KeyCode)
	{
		case KBDEV_KEY_UP_ARROW:    snprintf(Input + Offset, 16 - Offset, "\x1B[%sA", ArrowKeyModifiers()); return;
		case KBDEV_KEY_DOWN_ARROW:  snprintf(Input + Offset, 16 - Offset, "\x1B[%sB", ArrowKeyModifiers()); return;
		case KBDEV_KEY_RIGHT_ARROW: snprintf(Input + Offset, 16 - Offset, "\x1B[%sC", ArrowKeyModifiers()); return;
		case KBDEV_KEY_LEFT_ARROW:  snprintf(Input + Offset, 16 - Offset, "\x1B[%sD", ArrowKeyModifiers()); return;
	}
	
	if (KeyCode >= KBDEV_KEY_F1 && KeyCode <= KBDEV_KEY_F4) {
		snprintf(Input + Offset, 16 - Offset, "\x1B%s%c", F1F4KeyModifiers(), 'P' + (KeyCode - KBDEV_KEY_F1));
		return;
	}
	
	if (KeyCode >= KBDEV_KEY_F5 && KeyCode <= KBDEV_KEY_F12) {
		snprintf(Input + Offset, 16 - Offset, "\x1B[%d%s~", FunctionKeyLookUp[KeyCode - KBDEV_KEY_F5], F5F12KeyModifiers());
		return;
	}
	
	if (IsAltPressed)
		strcpy(Input, "\x1B");
	
	if (IsCtrlPressed)
	{
		// ^A == 0x01, ^Z == 0x1A
		char Ascii = KeyboardMapNoShift[KeyCode];
		char Typed = Ascii;
		if (Ascii >= 'a' && Ascii <= 'z')
		{
			Typed = 1 + (Ascii - 'a');
		}
		else switch (Ascii)
		{
			case KBDEV_KEY_OEM_LEFT_BRACKET:   Typed = '\x1B'; break;
			case KBDEV_KEY_OEM_BACKSLASH:      Typed = '\x1C'; break;
			case KBDEV_KEY_OEM_RIGHT_BRACKET:  Typed = '\x1D'; break;
			case KBDEV_KEY_2:                  if (IsShiftPressed) { Typed = '\x00'; } break; // note: this won't work
			case KBDEV_KEY_6:                  if (IsShiftPressed) { Typed = '\x1E'; } break;
			case KBDEV_KEY_MINUS:              if (IsShiftPressed) { Typed = '\x1F'; } break;
		}
		
		Input[Offset++] = Typed;
	}
	else if (IsShiftPressed && KeyCode < ARRAY_COUNT(KeyboardMapShift))
	{
		Input[Offset++] = KeyboardMapShift[KeyCode];
	}
	else if (KeyCode < ARRAY_COUNT(KeyboardMapNoShift))
	{
		Input[Offset++] = KeyboardMapNoShift[KeyCode];
	}
	
	Input[Offset++] = 0;
}

BSTATUS UseKeyboard(const char* KeyboardName)
{
	BSTATUS Status;
	OBJECT_ATTRIBUTES Attributes;
	Attributes.ObjectName = KeyboardName;
	Attributes.ObjectNameLength = strlen(KeyboardName);
	Attributes.OpenFlags = OB_OPEN_OBJECT_NAMESPACE;
	Attributes.RootDirectory = HANDLE_NONE;
	
	Status = OSOpenFile(&KeyboardHandle, &Attributes);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Couldn't open keyboard '%s'. %s (%d)", KeyboardName, RtlGetStatusString(Status), Status);
		return Status;
	}
	
	return Status;
}
