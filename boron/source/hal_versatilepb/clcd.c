/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/clcd.c
	
Abstract:
	This module implements CLCD framebuffer support.
	
Author:
	iProgramInCpp - 31 January 2026
***/
#include <ke.h>
#include <mm.h>
#include <string.h>

#ifdef TARGET_ARM

#define PL110_BASE (0x10120000)

typedef volatile struct
{
	/* 0x00 */ uint32_t LcdTiming0;
	/* 0x04 */ uint32_t LcdTiming1;
	/* 0x08 */ uint32_t LcdTiming2;
	/* 0x0C */ uint32_t LcdTiming3; // 17 bit
	/* 0x10 */ uint32_t LcdUPBase;
	/* 0x14 */ uint32_t LcdLPBase;
	/* 0x18 */ uint32_t LcdControl; // 16 bit
	/* 0x1C */ uint32_t LcdImsc;    // 5 bit
	/* 0x20 */ uint32_t LcdRis;     // 5 bit
	/* 0x24 */ uint32_t LcdMis;     // 5 bit
	/* 0x28 */ uint32_t LcdIcr;     // 5 bit
	/* 0x2C */ uint32_t LcdUPCurr;
	/* 0x30 */ uint32_t LcdLPCurr;
	/* 0x034 - 0x1FC */ uint32_t Reserved[0x73];
	/* 0x200 - 0x3FC */ uint32_t LcdPalette[0x80];
	/* 0x400 - 0xFDC */ uint32_t Reserved2[0x2F8];
	/* 0xFE0 */ uint32_t ClcdPeriphId0;
	/* 0xFE4 */ uint32_t ClcdPeriphId1;
	/* 0xFE8 */ uint32_t ClcdPeriphId2;
	/* 0xFEC */ uint32_t ClcdPeriphId3;
	/* 0xFF0 */ uint32_t ClcdPCellId0;
	/* 0xFF4 */ uint32_t ClcdPCellId1;
	/* 0xFF8 */ uint32_t ClcdPCellId2;
	/* 0xFFC */ uint32_t ClcdPCellId3;
}
PL111_MMIO, *PPL111_MMIO;

static_assert(sizeof(PL111_MMIO) == 4096);

static volatile PPL111_MMIO HalPL111;

#define TIM0_PPL(pixelsPerLine) ((((pixelsPerLine) / 16) - 1) << 2)
#define TIM0_HBP(backPorch) ((backPorch) << 24)
#define TIM0_HFP(frontPorch) ((frontPorch) << 16)
#define TIM0_HSW(pulseWidth) ((pulseWidth) << 8)

#define TIM1_VBP(backPorch) ((backPorch) << 24)
#define TIM1_VFP(frontPorch) ((frontPorch) << 16)
#define TIM1_VSW(pulseWidth) ((pulseWidth) << 10)
#define TIM1_LPP(linesPerPanel) ((linesPerPanel) - 1)

#define TIM2_PCD_HI(pcd) ((((pcd) >> 5) & 0x1F) << 27)
#define TIM2_PCD_LO(pcd) ((pcd) & 0x1F)
#define TIM2_PCD(pcd) TIM2_PCD_HI(pcd) | TIM2_PCD_LO(pcd)
#define TIM2_BCD (1 << 26)
#define TIM2_CPL(cpl) ((cpl) << 16)
#define TIM2_IEO (1 << 14)
#define TIM2_IPC (1 << 13)
#define TIM2_IHS (1 << 12)
#define TIM2_IVS (1 << 11)
#define TIM2_ACB(acb) ((acb) << 6)
#define TIM2_CLKSEL (1 << 5)

#define CTL_LCDEN   (1 << 0)
#define CTL_16BPP   (0b100 << 1)
#define CTL_24BPP   (0b101 << 1)
#define CTL_LCDBW   (1 << 4)
#define CTL_LCDTFT  (1 << 5)
#define CTL_LCDM8   (1 << 6)
#define CTL_LCDDUAL (1 << 7)
#define CTL_LCDBGR  (1 << 8)
#define CTL_LCDBEBO (1 << 9)  // big endian byte order
#define CTL_LCDBEPO (1 << 10) // big endian pixel order
#define CTL_LCDPWR  (1 << 11)

uint16_t HalFramebuffer[640 * 480];

void HalVpbInitCLCD()
{
	HalPL111 = MmMapIoSpace(PL110_BASE, 4096, MM_PTE_READWRITE, POOL_TAG("CLCD"));
	if (!HalPL111)
		KeCrash("ERROR: Cannot map CLCD");
	
	DbgPrint("HalPL111: %p", HalPL111);
	
	HalPL111->LcdControl = 0;
	
	// set up a 640x480x16 bpp framebuffer
	HalPL111->LcdTiming0 = TIM0_PPL(640) | TIM0_HBP(63) | TIM0_HFP(31) | TIM0_HSW(95);
	HalPL111->LcdTiming1 = TIM1_LPP(480) | TIM1_VBP(8) | TIM1_VFP(11) | TIM1_VSW(24);
	HalPL111->LcdTiming2 = TIM2_PCD(0) | TIM2_BCD | TIM2_IEO; // TODO: PCD ignored in QEMU, not on an actual board.
	HalPL111->LcdTiming3 = 0;
	
	// TODO: pick a sane frame buffer location
	HalPL111->LcdUPBase = ((uintptr_t) HalFramebuffer - 0xC0000000);
	HalPL111->LcdLPBase = ((uintptr_t) HalFramebuffer - 0xC0000000);
	
	HalPL111->LcdControl = CTL_LCDEN | CTL_16BPP | CTL_LCDTFT | CTL_LCDBGR | CTL_LCDPWR;
	
	DbgPrint("initialized CLCD");
	
	memset(HalFramebuffer, 0x34, 640 * 480 * 2);
}

#endif