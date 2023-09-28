/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/ipl.h
	
Abstract:
	This header file contains the IPL manipulation function
	prototypes.
	
Author:
	iProgramInCpp - 28 September 2023
***/
#ifndef BORON_KE_IPL_H
#define BORON_KE_IPL_H

// To raise the IPL temporarily, opt for the following structure;
//     eIPL oldIPL = KeRaiseIPL(IPL_WHATEVER);
//     .... your code here
//     KeLowerIPL(oldIPL);

// Raise the IPL of the current processor.
KIPL KeRaiseIPL(KIPL newIPL);

// Lower the IPL of the current processor back to the level before KeRaiseIPL.
KIPL KeLowerIPL(KIPL newIPL);

// Get the current IPL of the processor.
KIPL KeGetIPL();

#endif//BORON_KE_IPL_H