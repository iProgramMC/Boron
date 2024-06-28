/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/thread.h
	
Abstract:
	This header file defines the ETHREAD structure, which is an
	extension of KTHREAD, and is exposed by the object manager.
	
Author:
	iProgramInCpp - 26 November 2023
***/
#pragma once

#include <ke/thread.h>

typedef struct ETHREAD_tag
{
	KTHREAD Tcb;
	
	LIST_ENTRY PendingIrpList;
	
	// TODO
}
ETHREAD, *PETHREAD;
