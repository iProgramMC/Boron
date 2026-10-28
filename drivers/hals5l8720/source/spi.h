/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/spi.h

Abstract:
	This header defines SPI-related definitions.

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 28 October 2026
***/
#pragma once

// Base addresses
#define SPI0_BASE 0x3C300000
#define SPI1_BASE 0x3CE00000
#define SPI2_BASE 0x3D200000
#define SPI3_BASE 0x3DA00000
#define SPI4_BASE 0x3E100000

#define SPI_CTRL     0x00
#define SPI_SETUP    0x04
#define SPI_STATUS   0x08
#define SPI_PIN      0x0C
#define SPI_TXDATA   0x10
#define SPI_RXDATA   0x20
#define SPI_CLKDIV   0x30
#define SPI_SPCNT    0x34
#define SPI_SPIDD    0x38
#define SPI_UNKREG4  0x3C
#define SPI_UNKREG5  0x40
#define SPI_UNKREG6  0x44
#define SPI_UNKREG7  0x48
#define SPI_TXBUFLEN 0x4C

#define SPI_SETUP_NO_TRANSMIT_JUNK       (1 << 0)
#define SPI_SETUP_LAST_CLOCKEDGE_MISSING (1 << 1)
#define SPI_SETUP_ACTIVE_LOW             (1 << 2)
#define SPI_SETUP_MASTER                 (1 << 3)
#define SPI_SETUP_OPTIONS                (1 << 5)
#define SPI_SETUP_UNKNOWN_1              (1 << 7)
#define SPI_SETUP_UNKNOWN_2              (1 << 8)
#define SPI_SETUP_CLOCK_SOURCE           (1 << 14)
#define SPI_SETUP_WORD_SIZE              (1 << 15) // 2 bit
#define SPI_SETUP_UNKNOWN_3              (1 << 21)

// Values
#define MAX_TX_BUFFER 16
#define TX_BUFFER_LEFT(x) (((x) >> 6) & 0x1F)
#define RX_BUFFER_LEFT(x) (((x) >> 11) & 0x1F)

#define CLOCK_SHIFT 14
#define WORD_SIZE_SHIFT 15
#define MAX_DIVIDER 0x3FF

#define SPI0_CLOCK_GATE 0x0E
#define SPI1_CLOCK_GATE 0x0F
#define SPI2_CLOCK_GATE 0x10
#define SPI3_CLOCK_GATE 0x11
#define SPI4_CLOCK_GATE 0x12

#define SPI0_IRQ 0x9
#define SPI1_IRQ 0xA
#define SPI2_IRQ 0xB
#define SPI3_IRQ 0x1C
#define SPI4_IRQ 0x37

#include "gpiopins.h"

#define SPI_PORT_COUNT 5

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
