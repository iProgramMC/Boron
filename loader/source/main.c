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
#include "requests.h"

void LdrStartup()
{
	InitializeMapper();
	DumpMemMap();
	LoadModules();
	
	
	LogMsg("Hi");
	
	ASM("cli");
	while (true)
		ASM("hlt");
}
