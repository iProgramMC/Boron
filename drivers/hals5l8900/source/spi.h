/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/spi.h

Abstract:
	This header defines SPI-related definitions.

Author:
	iProgramInCpp - 24 October 2026
***/
#pragma once

// Base addresses
#define SPI0_BASE 0x3C300000
#define SPI1_BASE 0x3CE00000
#define SPI2_BASE 0x3D200000

#define SPI_CTRL   0x00
#define SPI_SETUP  0x04
#define SPI_STATUS 0x08
#define SPI_PIN    0x0C
#define SPI_TXDATA 0x10
#define SPI_RXDATA 0x20
#define SPI_CLKDIV 0x30
#define SPI_SPCNT  0x34
#define SPI_SPIDD  0x38

// Values
#define MAX_TX_BUFFER 8
#define TX_BUFFER_LEFT(x) (((x) >> 4) & 0xF)
#define RX_BUFFER_LEFT(x) (((x) >> 8) & 0xF)

#define CLOCK_SHIFT 12
#define MAX_DIVIDER 0x3FF

#define SPI0_CLOCK_GATE 0x22
#define SPI1_CLOCK_GATE 0x2B
#define SPI2_CLOCK_GATE 0x2F

#define SPI0_IRQ 0x9
#define SPI1_IRQ 0xA
#define SPI2_IRQ 0xB

// weird mappings, why couldn't they stay the same across versions!?
#define GPIO_SPI0_CS0_IPH    0x400
#define GPIO_SPI0_CS0_IPT1   0x700

#define GPIO_SPI0_CS1_IPH3G  0x705
#define GPIO_SPI0_CS2_IPH3G  0x706

#define GPIO_SP1_CS0         0x1800

#define GPIO_SPI2_CS0_IPT1   0x1804
#define GPIO_SPI2_CS1_IPT1   0x705
#define GPIO_SPI2_CS0_IPH    0x705

#define SPI_PORT_COUNT 3

typedef struct
{
	// OS Interface
	uint8_t* BaseAddress;
	KINTERRUPT Interrupt;
	KSPIN_LOCK SpinLock;

	// Hardware info
	char PortNumber;
	char WordSize;
	volatile bool TxDone;
	volatile bool RxDone;

	bool IsActiveLow;
	bool LastClockEdgeMissing;
	char ClockSource;
	bool IsMaster;

	int BaudRate;
	int Option5;

	const volatile uint8_t* TxBuffer;
	volatile uint8_t* RxBuffer;
	volatile size_t TxCurrentLen;
	volatile size_t TxTotalLen;
	volatile size_t RxCurrentLen;
	volatile size_t RxTotalLen;
	volatile size_t Counter;

	size_t SetupOptions;
}
SPI_INFO, *PSPI_INFO;

void HalInitSpi();
