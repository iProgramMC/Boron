/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ttys.h
	
Abstract:
	This header file contains the publicly exposed structure
	definitions for Boron's terminal subsystem.
	
Author:
	iProgramInCpp - 21 November 2025
***/
#pragma once

#include <main.h>

typedef struct
{
	struct
	{
		// ==== Local Flags ====
		// If set, inputs will be forwarded to the terminal as they are.  If not, the
		// terminal is in a mode known as "canonical" mode.
		unsigned RawMode : 1;
		
		// If set, an interrupt signal will be sent when the break character is received.
		unsigned InterruptOnBreak : 1;
		
		// If set, each input will be echoed (in a formatted way) back to the host. For
		// example, if the host sends "aaa", then Ctrl-A, then "bbb", while the input
		// stream would be "aaa\u0001bbb", the output would be "aaa^Abbb".
		unsigned Echo : 1;
	}
	PACKED
	Local;
	
	struct
	{	
		// ==== Input Flags ====
		// If set, carriage return characters are ignored entirely.
		unsigned IgnoreCR : 1;
		
		// If set, carriage return (ASCII 13) characters are turned into line feed
		// (ASCII 10) characters.
		unsigned ConvertCRToNL : 1;
		
		// If set, line feed (ASCII 10) characters are turned into carriage return
		// (ASCII 13) characters.
		unsigned ConvertNLToCR : 1;
		
		// If set, input is treated as UTF-8, and UTF-8 codepoints are erased completely.
		unsigned InputIsUTF8 : 1;
		
		// TODO: IUCLC, IXOFF, IXON, and IXANY.
	}
	PACKED
	Input;
	
	struct
	{
		// ==== Output Flags ====
		// If set, line feed (ASCII 10) characters are mapped to a combination of the
		// carriage return (ASCII 13) and the line feed (ASCII 10) characters.
		unsigned ConvertNLToCRNL : 1;
		
		// If set, carriage return (ASCII 10) characters are turned into line feed
		// (ASCII 13) characters.
		unsigned ConvertCRToNLOutput : 1;
		
		// TODO
	}
	PACKED
	Output;
	
	struct
	{
		char EndOfFile;
		char EndOfLine;
		char EndOfLine2;
		char Erase;
		char Interrupt;
		char EraseLine;
		char EraseWord;
		char Suspend;
		
		// TODO: VSTOP, VSTART, etc.
	}
	Chars;
}
TERMINAL_STATE, *PTERMINAL_STATE;

typedef struct
{
	int Width;
	int Height;
}
TERMINAL_WINDOW_SIZE, *PTERMINAL_WINDOW_SIZE;
