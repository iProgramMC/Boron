/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	rtl/ansi.h
	
Abstract:
	This header file contains ANSI escape code definitions.
	
Author:
	iProgramInCpp - 25 October 2023
***/
#ifndef BORON_RTL_ANSI_H
#define BORON_RTL_ANSI_H

#define ANSI_HOME      "\x1b[H"
#define ANSI_POS(x,y) ("\x1b[" #x ";" #y "H")
#define ANSI_CLEAR     "\x1b[2J"

#define ANSI_RESET   "\x1b[0m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_DEFAULT "\x1b[39m"
#define ANSI_BLACK   "\x1b[90m"
#define ANSI_RED     "\x1b[91m"
#define ANSI_GREEN   "\x1b[92m"
#define ANSI_YELLOW  "\x1b[93m"
#define ANSI_BLUE    "\x1b[94m"
#define ANSI_MAGENTA "\x1b[95m"
#define ANSI_CYAN    "\x1b[96m"
#define ANSI_WHITE   "\x1b[97m"

#endif//BORON_RTL_ANSI_H
