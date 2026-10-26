/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	spi.c

Abstract:
	xx

Author:
	iProgramInCpp - 26 October 2026
***/
#include "z2.h"

MT_SPI_SETTING MtNormalSpeed = {
	.Speed = 83000,
	.TxDelay = 5000,
	.RxDelay = 10000,
};

MT_SPI_SETTING MtFastSpeed = {
	.Speed = 4500000,
	.TxDelay = 0,
	.RxDelay = 10000,
};

int MtSpiTxRx(PMT_SPI_SETTING SpiSetting, const void* OutBuffer, size_t OutBufferLen, void* InBuffer, size_t InBufferLen)
{
	HalSetBaudRateSpi(MtSpiPort(), SpiSetting->Speed, 8, true, true, true);
	HalSetOutputPinGpio(MtSpiCsPin(), 0);
	OSSleep((SpiSetting->RxDelay + 999) / 1000);

	int Ret = HalTransmitAndReceiveSpi(MtSpiPort(), OutBuffer, OutBufferLen, InBuffer, InBufferLen, true);

	HalSetOutputPinGpio(MtSpiCsPin(), 1);
	return Ret;
}

int MtSpiTx(PMT_SPI_SETTING SpiSetting, const void* OutBuffer, size_t OutBufferLen)
{
	HalSetBaudRateSpi(MtSpiPort(), SpiSetting->Speed, 8, true, true, true);
	HalSetOutputPinGpio(MtSpiCsPin(), 0);
	OSSleep((SpiSetting->TxDelay + 999) / 1000);

	int Ret = HalTransmitSpi(MtSpiPort(), OutBuffer, OutBufferLen, true, 0);

	HalSetOutputPinGpio(MtSpiCsPin(), 1);
	return Ret;
}
