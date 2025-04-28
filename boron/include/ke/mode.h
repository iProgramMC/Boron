/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/mode.h
	
Abstract:
	This module defines the KPROCESSOR_MODE enum
	
Author:
	iProgramInCpp - 27 November 2023
***/
#pragma once

typedef enum KPROCESSOR_MODE_tag
{
	MODE_KERNEL,
	MODE_USER,
}
KPROCESSOR_MODE, *PKPROCESSOR_MODE;

// Gets the previous processor mode.  This is MODE_USER if a user
// system service request or page fault brought the thread back
// up to kernel mode.
KPROCESSOR_MODE KeGetPreviousMode();

// Sets the mode (kernel or user) that the kernel uses when
// validating addresses when calling into system services.
//
// Use this very carefully.  Only use this if ALL of the addresses
// you are about to pass into a system service are kernel mode
// addresses (such as part of the kernel stack or something).
KPROCESSOR_MODE KeSetAddressMode(KPROCESSOR_MODE NewMode);
