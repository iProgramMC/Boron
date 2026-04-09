/***
	Copyright (C) 2026 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the main function of the Boron minimal
	command-line interpreter.
	
Author:
	iProgramInCpp - 26 February 2026
***/
#include <boron.h>
#include <stdarg.h>

#include "command.h"

int main(UNUSED int ArgumentCount, UNUSED char** ArgumentArray)
{
	CmdPrintInitMessage();
	
	while (true)
	{
		CmdPrintPrompt();
		CmdReadInput();
		CmdParseCommand();
	}
}