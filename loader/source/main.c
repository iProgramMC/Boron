/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                           main.h                            *
\*************************************************************/
#include <loader.h>
#include "mapper.h"

void LdrStartup()
{
	InitializeMapper();
	DumpMemMap();
	
	LogMsg("Test");
	
	ASM("cli");
	while (true)
		ASM("hlt");
}
