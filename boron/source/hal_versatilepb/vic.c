/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/gic.c
	
Abstract:
	This module contains the driver for the VIC (PL190).
	
Author:
	iProgramInCpp - 31 December 2025
***/
#ifdef TARGET_ARM
#include <ke.h>
#include "hali.h"

#define VIC_BASE       ((volatile unsigned int*) 0xD1840000)
#define VIC_IRQ_STATUS (VIC_BASE[0])
#define VIC_FIQ_STATUS (VIC_BASE[1])
#define VIC_RAWINT_STA (VIC_BASE[2])
#define VIC_INT_SELECT (VIC_BASE[3])
#define VIC_INT_ENABLE (VIC_BASE[4])
#define VIC_INT_ENCLR  (VIC_BASE[5])
#define VIC_SWI        (VIC_BASE[6])
#define VIC_SWI_CLEAR  (VIC_BASE[7])
#define VIC_PROT       (VIC_BASE[8])
#define VIC_VECT_ADDR  (VIC_BASE[12])
#define VIC_DEFVECTADD (VIC_BASE[13])
#define VIC_VECT_ADDRS (&VIC_BASE[64])
#define VIC_VECT_CNTLS (&VIC_BASE[128])
#define VIC_ITCR       (VIC_BASE[192])
#define VIC_ITIP1      (VIC_BASE[193])
#define VIC_ITIP2      (VIC_BASE[194])
#define VIC_ITOP1      (VIC_BASE[195])
#define VIC_ITOP2      (VIC_BASE[196])
#define VIC_PERIPHID0  (VIC_BASE[1016])
#define VIC_PERIPHID1  (VIC_BASE[1017])
#define VIC_PERIPHID2  (VIC_BASE[1018])
#define VIC_PERIPHID3  (VIC_BASE[1019])
#define VIC_CELLID0    (VIC_BASE[1020])
#define VIC_CELLID1    (VIC_BASE[1021])
#define VIC_CELLID2    (VIC_BASE[1022])
#define VIC_CELLID3    (VIC_BASE[1023])

void HalVpbEndOfInterrupt(int InterruptNumber)
{
	DbgPrint("TODO %s", __func__);
}

void HalVpbRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector)
{
	DbgPrint("TODO %s", __func__);
}

void HalVpbRegisterInterruptHandler(int Irq, void(*Func)())
{
	DbgPrint("TODO %s", __func__);
}

PKREGISTERS HalVpbOnInterruptRequest(PKREGISTERS Registers)
{
	DbgPrint("TODO %s", __func__);
	return Registers;
}

PKREGISTERS HalVpbOnFastInterruptRequest(PKREGISTERS Registers)
{
	DbgPrint("TODO %s", __func__);
	return Registers;
}

int HalVpbGetMaximumInterruptCount()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

void HalInitPL190()
{
	DbgPrint("TODO %s", __func__);
}

void HalInitTimer()
{
	DbgPrint("TODO %s", __func__);
}

#endif
