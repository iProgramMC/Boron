//
//  main.cpp
//
//  Copyright (C) 2026 iProgramInCpp.
//  Created on 9/4/2026.
//
#include <boron.h>
#include <stdarg.h>
#include "frg.h"
#include "File.hpp"
#include "Screen.hpp"

void Usage()
{
	DbgPrint("Desktop - help");
	DbgPrint("	--framebuffer [fileName]: The frame buffer device to which this desktop instance will output.");
	DbgPrint("	--keyboard [fileName]:    The keyboard from which this desktop instance will read user input.");
	DbgPrint("	--keyboard [fileName]:    The keyboard from which this desktop instance will read user input.");
}

int main(UNUSED int ArgumentCount, UNUSED char** ArgumentArray)
{
	BSTATUS Status;
	
	char* FramebufferName = NULL, *KeyboardName = NULL;
	
	for (int i = 1; i < ArgumentCount; i++)
	{
		if (strcmp(ArgumentArray[i], "--framebuffer") == 0)
			FramebufferName = ArgumentArray[i + 1];
		if (strcmp(ArgumentArray[i], "--keyboard") == 0)
			KeyboardName = ArgumentArray[i + 1];
	}
	
	if (!FramebufferName)
	{
		DbgPrint("ERROR: No framebuffer specified.");
		Usage();
		return 1;
	}
	
	if (!KeyboardName)
	{
		DbgPrint("ERROR: No keyboard specified.");
		Usage();
		return 1;
	}
	
	File Framebuffer(FramebufferName, OB_OPEN_OBJECT_NAMESPACE);
	Status = Framebuffer.GetOpenStatus();
	if (FAILED(Status))
	{
		DbgPrint("Error opening frame buffer '%s': %s", FramebufferName, RtlGetStatusString(Status));
		return 1;
	}
	
	Screen MainScreen(Framebuffer.GetHandle());
	
	
	return 0;
}