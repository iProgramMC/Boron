/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	spi.c

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 10 November 2026
***/
#include "z1.h"

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

MT_SPI_SETTING MtReaderSpeed = {
	.Speed = 83000,
	.TxDelay = 1000,
	.RxDelay = 1000
};

int MtSpiTxRx(PMT_SPI_SETTING SpiSetting, const void* OutBuffer, size_t OutBufferLen, void* InBuffer, size_t InBufferLen)
{
	HalSetBaudRateSpi(MT_SPI_PORT, SpiSetting->Speed, 8, true, true, true);
	HalSetOutputPinGpio(MT_SPI_CS, 0);

	if (SpiSetting->RxDelay) {
		OSSleep((SpiSetting->RxDelay + 999) / 1000);
	}

	int Ret = HalTransmitAndReceiveSpi(MT_SPI_PORT, OutBuffer, OutBufferLen, InBuffer, InBufferLen, true);

	HalSetOutputPinGpio(MT_SPI_CS, 1);
	return Ret;
}

int MtSpiTx(PMT_SPI_SETTING SpiSetting, const void* OutBuffer, size_t OutBufferLen)
{
	HalSetBaudRateSpi(MT_SPI_PORT, SpiSetting->Speed, 8, true, true, true);
	HalSetOutputPinGpio(MT_SPI_CS, 0);

	if (SpiSetting->TxDelay)
		OSSleep((SpiSetting->TxDelay + 999) / 1000);

	int Ret = HalTransmitSpi(MT_SPI_PORT, OutBuffer, OutBufferLen, true, 0);

	HalSetOutputPinGpio(MT_SPI_CS, 1);
	return Ret;
}
