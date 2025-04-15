/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/process.h
	
Abstract:
	This header file defines the PEB structure, also known
	as the Process Environment Block.  This structure is
	likely going to be one page (4 KB) long and offers
	information about the created process.  Most of it is
	given through the OSSetProcessEnvironmentData() system
	service.
	
	It also defines the TEB (Thread Environment Block) struct
	created for each thread.
	
Author:
	iProgramInCpp - 15 April 2025
***/
#pragma once

#define MAX_COMMAND_LINE_LEN (4096)

typedef struct
{
	void *UserProcessParameters;
	
	// TODO: need anything more?
}
PEB, *PPEB;

typedef struct
{
	PPEB Peb;
}
TEB, *PTEB;
