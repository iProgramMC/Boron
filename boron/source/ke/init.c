/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/init.c
	
Abstract:
	This module implements the system startup function.
	
Author:
	iProgramInCpp - 28 August 2023
***/

#include <main.h>
#include <hal.h>
#include <mm.h>
#include <ldr.h>
#include "ki.h"
#include <rtl/symdefs.h>

int colors[] = {
	0b1111100000000000, // 0-RED
	0b1111101111100000, // 1-ORANGE
	0b1111111111100000, // 2-YELLOW
	0b1111100000011111, // 3-GREENYELLOW
	0b0000011111100000, // 4-GREEN
	0b1111100000011111, // 5-GREENBLUE
	0b0000011111111111, // 6-CYAN
	0b0000001111111111, // 7-TURQUOISE
	0b0000000000011111, // 8-BLUE
	0b1111100000011111, // 9-MAGENTA
	0b1111100000000000, // 10-RED
	0b1111101111100000, // 11-ORANGE
	0b1111111111100000, // 12-YELLOW
	0b0111111111100000, // 13-GREENYELLOW
	0b0000011111100000, // 14-GREEN
	0b0000011111101111, // 15-GREENBLUE//skipped this one!?
	0b0000011111111111, // 16-CYAN
	0b0000001111111111, // 17-TURQUOISE
	0b0000000000011111, // 18-BLUE
	0b1111100000011111, // 19-MAGENTA
};

void Marker(int i)
{
	extern char KiRootPageTable[];
	int* a;
	if (KeGetCurrentPageTable() == (uintptr_t) &KiRootPageTable) {
		a = (int*)(0x400000);
	}
	else {
		a = (int*)(0xCFD00000);
	}
	
	int pattern = colors[i] | (colors[i] << 16);
	a[i * 4 + 3] = a[i * 4 + 2] = a[i * 4 + 1] = a[i * 4 + 0] = pattern;
	
	//const int stopAt = 7;
	//if (stopAt <= i) {
	//	while (true) {
	//		ASM("wfi");
	//	}
	//}
}

// The entry point to our kernel.
NO_RETURN INIT
void KiSystemStartup(void)
{
	Marker(0);
	DbgInit();
	Marker(1);
	KiInitLoaderParameterBlock();
	Marker(2);
	MiInitPMM();
	Marker(3);
	MmInitAllocators();
	Marker(4);
	KeSchedulerInitUP();
	Marker(5);
	KeInitArchUP();
	Marker(6);
	LdrInit();
	Marker(7);
	LdrInitializeHal();
	Marker(8);
	HalInitSystemUP();
	Marker(9);
	LdrInitAfterHal();
	Marker(10);
	KeInitSMP(); // no return
}
