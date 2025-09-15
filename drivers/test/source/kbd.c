/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	kbd.c
	
Abstract:
	This module implements a small keyboard key code translator
	for testing purposes.  Used by kbdtst.c and pipetst.c.
	I8042 specific for now.
	
Author:
	iProgramInCpp - 15 September 2025
***/
#include "utils.h"

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
  'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r',	/* Enter key */
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

#define KEY_LSHIFT 0x2a
#define KEY_RSHIFT 0x36

bool IsShiftPressed = false;

char TranslateKeyCode(char KeyCode)
{
	if (KeyCode == KEY_LSHIFT || KeyCode == KEY_RSHIFT) {
		IsShiftPressed = true;
		return 0;
	}
	
	bool Released = KeyCode & 0x80;
	KeyCode &= ~0x80;
	
	if (KeyCode == KEY_LSHIFT || KeyCode == KEY_RSHIFT) {
		IsShiftPressed = false;
		return 0;
	}
	
	// If the key was released
	if (Released)
		return 0;
	
	return KeyboardMap[IsShiftPressed * 128 + KeyCode];
}
