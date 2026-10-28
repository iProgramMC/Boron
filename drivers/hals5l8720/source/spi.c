/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/spi.c

Abstract:
	This module contains the implementation of the SPI
	driver for the S5L8720 platform.

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
#include "hali.h"
#include <mm.h>
#include "gpio.h"
#include "spi.h"
#include "clock.h"

#ifdef CONFIG_SMP
#error TODO: synchronize with KeSynchronizeInterrupt instead of disabling interrupts!
#endif

static const uintptr_t HalSpiAddresses[SPI_PORT_COUNT] = {
	SPI0_BASE,
	SPI1_BASE,
	SPI2_BASE,
	SPI3_BASE,
	SPI4_BASE
};
static const char HalSpiIrqNumbers[SPI_PORT_COUNT] = {
	SPI0_IRQ,
	SPI1_IRQ,
	SPI2_IRQ,
	SPI3_IRQ,
	SPI4_IRQ
};
static const char HalSpiClockGates[SPI_PORT_COUNT] = {
	SPI0_CLOCK_GATE,
	SPI1_CLOCK_GATE,
	SPI2_CLOCK_GATE,
	SPI3_CLOCK_GATE,
	SPI4_CLOCK_GATE
};
static const int HalWordSizes[] = { 1, 2, 4 };

static SPI_INFO HalSpiInfo[SPI_PORT_COUNT];

void HalSpiInterruptHandler(PKINTERRUPT Interrupt, void* Context);

void HalInitSpi()
{
	for (int i = 0; i < SPI_PORT_COUNT; i++)
	{
		HalSetEnabledClockGate(HalSpiClockGates[i], true);
	}

	for (int i = 0; i < SPI_PORT_COUNT; i++)
	{
		HalSpiInfo[i].PortNumber = i;

		HalSpiInfo[i].BaseAddress = MmMapIoSpace(
			HalSpiAddresses[i],
			PAGE_SIZE,
			MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
			POOL_TAG("SpiP")
		);

		if (!HalSpiInfo[i].BaseAddress)
			KeCrash("Hal: Could not map SPI ports");

		HalSpiInfo[i].ClockSource = HAL_SPI_CLOCKSOURCE_NCLK;

		REG(HalSpiInfo[i].BaseAddress, SPI_CTRL) = 0;

		KeInitializeInterrupt(
			&HalSpiInfo[i].Interrupt,
			HalSpiInterruptHandler,
			&HalSpiInfo[i],
			&HalSpiInfo[i].SpinLock,
			HalSpiIrqNumbers[i],
			IPL_DEVICES0, // InterruptIpl
			false         // SharedVector
		);
	}

	for (int i = 0; i < SPI_PORT_COUNT; i++)
	{
		KeConnectInterrupt(&HalSpiInfo[i].Interrupt);
	}
}

// NOTE: Must be synchronized with the interrupt.
static void HalSpiTxData(PSPI_INFO Port, const volatile void* Buffer, size_t From, size_t To)
{
	if (To - From == 0)
		return;

	size_t FromW = From >> Port->WordSize;
	size_t ToW = To >> Port->WordSize;
	void* Base = Port->BaseAddress;

	const uint8_t* Buffer8 = (const uint8_t*) Buffer;
	const uint16_t* Buffer16 = (const uint16_t*) Buffer;
	const uint32_t* Buffer32 = (const uint32_t*) Buffer;

	switch (Port->WordSize)
	{
		case 0: // 8-bit
		{
			for (size_t i = FromW; i < ToW; i++)
				REG(Base, SPI_TXDATA) = Buffer8[i];

			break;
		}
		case 1: // 16-bit
		{
			for (size_t i = FromW; i < ToW; i++)
				REG(Base, SPI_TXDATA) = Buffer16[i];

			break;
		}
		case 2: // 32-bit
		{
			for (size_t i = FromW; i < ToW; i++)
				REG(Base, SPI_TXDATA) = Buffer32[i];

			break;
		}
	}
}

// NOTE: Must be synchronized with the interrupt.
static void HalSpiRxData(PSPI_INFO Port, volatile void* Buffer, size_t From, size_t To)
{
	if (To - From == 0)
		return;

	size_t FromW = From >> Port->WordSize;
	size_t ToW = To >> Port->WordSize;
	void* Base = Port->BaseAddress;

	uint8_t* Buffer8 = (uint8_t*) Buffer;
	uint16_t* Buffer16 = (uint16_t*) Buffer;
	uint32_t* Buffer32 = (uint32_t*) Buffer;

	switch (Port->WordSize)
	{
		case 0: // 8-bit
		{
			for (size_t i = FromW; i < ToW; i++)
				Buffer8[i] = REG(Base, SPI_RXDATA);

			break;
		}
		case 1: // 16-bit
		{
			for (size_t i = FromW; i < ToW; i++)
				Buffer16[i] = REG(Base, SPI_RXDATA);

			break;
		}
		case 2: // 32-bit
		{
			for (size_t i = FromW; i < ToW; i++)
				Buffer32[i] = REG(Base, SPI_RXDATA);

			break;
		}
	}
}

int HalTransmitSpi(int Port, const void* Buffer, size_t BufferSize, bool Block, int Unknown)
{
	if (Port < 0 || Port >= SPI_PORT_COUNT) {
		DbgPrint("HalTransmitSpi: Invalid port %d", Port);
		return -1;
	}

	bool Restore = KeDisableInterrupts();

	void* Base = HalSpiInfo[Port].BaseAddress;

	REG(Base, SPI_CTRL) = REG(Base, SPI_CTRL) | (1 << 2);
	REG(Base, SPI_CTRL) = REG(Base, SPI_CTRL) | (1 << 3);

	HalSpiInfo[Port].RxBuffer = NULL;
	HalSpiInfo[Port].RxDone = false;
	HalSpiInfo[Port].RxCurrentLen = 0;
	HalSpiInfo[Port].RxTotalLen = 0;

	HalSpiInfo[Port].TxBuffer = Buffer;

	if (BufferSize > MAX_TX_BUFFER)
		HalSpiInfo[Port].TxCurrentLen = MAX_TX_BUFFER;
	else
		HalSpiInfo[Port].TxCurrentLen = BufferSize;

	HalSpiInfo[Port].TxTotalLen = BufferSize;
	HalSpiInfo[Port].TxDone = false;

	if (!Unknown) {
		REG(Base, SPI_SPCNT) = 0;
	}
	else {
		HalSpiInfo[Port].SetupOptions |= SPI_SETUP_UNKNOWN_1;
	}

	HalSpiTxData(&HalSpiInfo[Port], Buffer, 0, HalSpiInfo[Port].TxCurrentLen);

	REG(Base, SPI_TXBUFLEN) = BufferSize;
	HalSpiInfo[Port].SetupOptions |= SPI_SETUP_UNKNOWN_2 | SPI_SETUP_UNKNOWN_3;
	REG(Base, SPI_SETUP) = HalSpiInfo[Port].SetupOptions;

	REG(Base, SPI_CTRL) = 1;

	KeRestoreInterrupts(Restore);

	if (!Block)
		return 0;

	// TODO: Don't spin in a loop, but wait for an event to signal.
	while (!HalSpiInfo[Port].TxDone || TX_BUFFER_LEFT(REG(Base, SPI_STATUS)) != 0) {
		KeSpinningHint();
	}

	return (int) BufferSize;
}

int HalReceiveSpi(int Port, void* Buffer, size_t BufferSize, bool Block, bool DontTransmitJunk)
{
	if (Port < 0 || Port >= SPI_PORT_COUNT) {
		DbgPrint("HalReceiveSpi: Invalid port %d", Port);
		return -1;
	}

	bool Restore = KeDisableInterrupts();

	void* Base = HalSpiInfo[Port].BaseAddress;

	REG(Base, SPI_CTRL) = REG(Base, SPI_CTRL) | (1 << 2);
	REG(Base, SPI_CTRL) = REG(Base, SPI_CTRL) | (1 << 3);

	HalSpiInfo[Port].RxBuffer = Buffer;
	HalSpiInfo[Port].RxDone = false;
	HalSpiInfo[Port].RxCurrentLen = 0;
	HalSpiInfo[Port].RxTotalLen = BufferSize;
	HalSpiInfo[Port].Counter = 0;

	HalSpiInfo[Port].TxBuffer = NULL;
	HalSpiInfo[Port].TxDone = false;
	HalSpiInfo[Port].TxTotalLen = 0;
	HalSpiInfo[Port].TxCurrentLen = 0;

	int WordType = HalSpiInfo[Port].WordSize;
	REG(Base, SPI_SPCNT) = (BufferSize + HalWordSizes[WordType]) >> WordType;
	REG(Base, SPI_CTRL) = 1;

	if (!DontTransmitJunk) {
		HalSpiInfo[Port].SetupOptions |= SPI_SETUP_NO_TRANSMIT_JUNK;
	}
	else {
		REG(Base, SPI_TXBUFLEN) = BufferSize;
		HalSpiInfo[Port].SetupOptions |= SPI_SETUP_UNKNOWN_3;
	}

	HalSpiInfo[Port].SetupOptions |= SPI_SETUP_UNKNOWN_1 | SPI_SETUP_UNKNOWN_2;
	REG(Base, SPI_SETUP) = HalSpiInfo[Port].SetupOptions;

	KeRestoreInterrupts(Restore);

	if (!Block) {
		return 0;
	}

	uint64_t StartTime = HalGetTickCount();
	while (!HalSpiInfo[Port].RxDone)
	{
		// TODO: Don't spin wait, but instead, wait for an event to signal.
		KeSpinningHint();

		// NOTE: OpeniBoot waits 1000 microseconds (1ms) here.
		// We'll just hardcode some bogus time that is probably longer.
		if (HalGetTickCount() - StartTime > 1000000)
		{
			// Stop it early and return -1.
			Restore = KeDisableInterrupts();

			HalSpiInfo[Port].RxDone = true;
			HalSpiInfo[Port].RxBuffer = NULL;

			if (!DontTransmitJunk) {
				HalSpiInfo[Port].SetupOptions &= ~SPI_SETUP_NO_TRANSMIT_JUNK;
				REG(Base, SPI_SETUP) = HalSpiInfo[Port].SetupOptions;
			}

			KeRestoreInterrupts(Restore);
			return -1;
		}
	}

	if (!DontTransmitJunk) {
		HalSpiInfo[Port].SetupOptions &= ~SPI_SETUP_NO_TRANSMIT_JUNK;
		REG(Base, SPI_SETUP) = HalSpiInfo[Port].SetupOptions;
	}

	return BufferSize;
}

int HalTransmitAndReceiveSpi(int Port, const void* TxBuffer, size_t TxBufferLength, void* RxBuffer, size_t RxBufferLength, bool Block)
{
	if (Port < 0 || Port >= SPI_PORT_COUNT) {
		DbgPrint("HalTransmitAndReceiveSpi: Invalid port %d", Port);
		return -1;
	}

	bool Restore = KeDisableInterrupts();
	void* Base = HalSpiInfo[Port].BaseAddress;

	REG(Base, SPI_CTRL) = REG(Base, SPI_CTRL) | (1 << 2);
	REG(Base, SPI_CTRL) = REG(Base, SPI_CTRL) | (1 << 3);

	HalSpiInfo[Port].TxBuffer = TxBuffer;
	if (TxBufferLength > MAX_TX_BUFFER)
		HalSpiInfo[Port].TxCurrentLen = MAX_TX_BUFFER;
	else
		HalSpiInfo[Port].TxCurrentLen = TxBufferLength;
	HalSpiInfo[Port].TxTotalLen = TxBufferLength;
	HalSpiInfo[Port].TxDone = false;

	HalSpiInfo[Port].RxBuffer = RxBuffer;
	HalSpiInfo[Port].RxDone = false;
	HalSpiInfo[Port].RxCurrentLen = 0;
	HalSpiInfo[Port].RxTotalLen = RxBufferLength;
	HalSpiInfo[Port].Counter = 0;

	HalSpiTxData(&HalSpiInfo[Port], TxBuffer, 0, HalSpiInfo[Port].TxCurrentLen);

	int WordType = HalSpiInfo[Port].WordSize;
	REG(Base, SPI_SPCNT) = (RxBufferLength + HalWordSizes[WordType]) >> WordType;
	REG(Base, SPI_CTRL) = 1;

	KeRestoreInterrupts(Restore);

	if (!Block)
		return 0;

	// TODO: Don't spin in a loop, but wait for an event to signal.
	while (!HalSpiInfo[Port].TxDone || !HalSpiInfo[Port].RxDone || TX_BUFFER_LEFT(REG(Base, SPI_STATUS)) != 0) {
		KeSpinningHint();
	}

	return (int) RxBufferLength;
}

void HalSetBaudRateSpi(int Port, int BaudRate, int WordSize, bool IsMaster, bool IsActiveLow, bool LastClockEdgeMissing)
{
	if (Port < 0 || Port >= SPI_PORT_COUNT) {
		DbgPrint("HalSetBaudRateSpi: Invalid port %d", Port);
		return;
	}

	bool Restore = KeDisableInterrupts();
	void* Base = HalSpiInfo[Port].BaseAddress;

	REG(Base, SPI_CTRL) = 0;

	switch (WordSize)
	{
		case 8: HalSpiInfo[Port].WordSize = 0; break;
		case 16: HalSpiInfo[Port].WordSize = 1; break;
		case 32: HalSpiInfo[Port].WordSize = 2; break;
		default: KeCrash("HalSetBaudRateSpi: Invalid word size %d", WordSize);
	}

	HalSpiInfo[Port].IsActiveLow = IsActiveLow;
	HalSpiInfo[Port].LastClockEdgeMissing = LastClockEdgeMissing;

	uint32_t ClockFrequency;
	if (HalSpiInfo[Port].ClockSource == HAL_SPI_CLOCKSOURCE_PCLK)
		ClockFrequency = HalGetClockFrequency(FREQ_BASE_PERIPHERAL);
	else
		ClockFrequency = HalGetClockFrequency(FREQ_BASE_FIXED);

	uint32_t Divider = ClockFrequency / BaudRate;
	if (Divider == 0)
		Divider = 1;

	if (Divider > MAX_DIVIDER)
		KeCrash("HalSetBaudRateSpi: Baud rate is too low! (BaudRate = %d)", BaudRate);

	REG(Base, SPI_CLKDIV) = Divider;

	HalSpiInfo[Port].BaudRate = BaudRate;
	HalSpiInfo[Port].IsMaster = IsMaster;

	uint32_t Options = 0;

	if (LastClockEdgeMissing)
		Options |= SPI_SETUP_LAST_CLOCKEDGE_MISSING;
	if (IsActiveLow)
		Options |= SPI_SETUP_ACTIVE_LOW;
	if (IsMaster)
		Options |= 3 << 3;

	if (HalSpiInfo[Port].Option5)
		Options |= 0x2 << 5;
	else
		Options |= 0x1 << 5;

	Options |= HalSpiInfo[Port].ClockSource << CLOCK_SHIFT;
	Options |= HalSpiInfo[Port].WordSize << WORD_SIZE_SHIFT;

	HalSpiInfo[Port].SetupOptions = Options;
	REG(Base, SPI_SETUP) = Options;
	REG(Base, SPI_PIN) = 2;
	REG(Base, SPI_CTRL) = 1;

	KeRestoreInterrupts(Restore);
}

static void HalSpiProcessTx(PSPI_INFO Port, uint32_t Status)
{
	void* Base = Port->BaseAddress;
	if (!Port->TxBuffer)
		return;

	if (Port->TxCurrentLen < Port->TxTotalLen)
	{
		size_t ToTx = Port->TxTotalLen - Port->TxCurrentLen;
		size_t CanTx = (MAX_TX_BUFFER - TX_BUFFER_LEFT(Status)) << Port->WordSize;

		if (ToTx > CanTx)
			ToTx = CanTx;

		HalSpiTxData(Port, Port->TxBuffer, Port->TxCurrentLen, Port->TxCurrentLen + ToTx);
		Port->TxCurrentLen += ToTx;

		// @BUG: See above note.
		//REG(Base, SPI_CTRL) = 1;
	}
	else
	{
		Port->TxDone = true;
		Port->TxBuffer = NULL;

		Port->SetupOptions &= ~SPI_SETUP_UNKNOWN_3;
		REG(Base, SPI_SETUP) = Port->SetupOptions;
	}
}

static bool HalSpiProcessRx(PSPI_INFO Port, uint32_t Status)
{
	void* Base = Port->BaseAddress;
	if (!Port->RxBuffer)
		return false;

	size_t ToRx = Port->RxTotalLen - Port->RxCurrentLen;
	size_t CanRx = RX_BUFFER_LEFT(Status) << Port->WordSize;

	if (ToRx > CanRx)
		ToRx = CanRx;

	HalSpiRxData(Port, Port->RxBuffer, Port->RxCurrentLen, Port->RxCurrentLen + ToRx);
	Port->RxCurrentLen += ToRx;

	if (Port->RxCurrentLen < Port->RxTotalLen)
		return false;

	Port->RxDone = true;
	Port->RxBuffer = NULL;
	Port->SetupOptions &= ~SPI_SETUP_UNKNOWN_1;
	REG(Base, SPI_SETUP) = Port->SetupOptions;
	return true;
}

static void HalSpiProcessTxRx(PSPI_INFO Port, uint32_t Status, bool StartWithTx)
{
	bool ShouldDoTx = StartWithTx;
	while (true)
	{
		if (ShouldDoTx)
			HalSpiProcessTx(Port, Status);

		ShouldDoTx = true;

		if (!HalSpiProcessRx(Port, Status))
			break;
	}
}

void HalSpiInterruptHandler(UNUSED PKINTERRUPT Interrupt, void* Context)
{
	PSPI_INFO Port = Context;
	void* Base = Port->BaseAddress;

	uint32_t Status = REG(Base, SPI_STATUS);

	// Unlike OpeniBoot's S5L8900 version of the SPI driver, the S5L8720 driver
	// actually resets the status here.  I guess there was some kind of bug after all...
	REG(Base, SPI_STATUS) = Status;

	if (Status & (1 << 3))
	{
		Port->Counter++;
	}

	if (Status & (1 << 0))
	{
		if (Status & ((1 << 1) | (1 << 22))) {
			HalSpiProcessTxRx(Port, Status, true);
		}
		else {
			HalSpiProcessTxRx(Port, Status, false);
		}
	}
	else if (Status & ((1 << 1) | (1 << 22)))
	{
		HalSpiProcessTxRx(Port, Status, true);
	}

	if (!Port->TxBuffer && !Port->RxBuffer)
	{
		Port->SetupOptions &= ~SPI_SETUP_UNKNOWN_2;
		REG(Base, SPI_SETUP) = Port->SetupOptions;
	}
}
