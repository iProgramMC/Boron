/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/acpi.c
	
Abstract:
	This module contains the implementation of ACPI-adjacent
	functions.
	
Author:
	iProgramInCpp - 24 September 2023
***/
#include "acpi.h"
#include "../../arch/amd64/pio.h"

static PRSDP_DESCRIPTION HalpRsdp;
static PRSDT_TABLE       HalpRsdt;

// List of important SDTs
PSDT_HEADER HalSdtFacp, HalSdtHpet, HalSdtApic, HalSdtMcfg;

uint32_t AcpiRsdpCheckSum(RSDP_DESCRIPTION* Description)
{
	uint8_t* DescBytes = (uint8_t*)Description;
	uint32_t CheckSum = 0;
	size_t   Size = offsetof(RSDP_DESCRIPTION, Length);
	
	for (size_t i = 0; i < Size; i++)
		CheckSum += DescBytes[i];
	
	return CheckSum;
}

bool AcpiUsingXsdt()
{
	return HalpRsdp->Revision >= 2 && HalpRsdp->XsdtAddress != 0;
}

int AcpiRsdtGetEntryCount(PRSDT_TABLE Table)
{
	int Shift = 2;
	if (AcpiUsingXsdt())
		Shift = 3;
	
	return (int) ((Table->Header.Length - sizeof(Table->Header)) >> Shift);
}

static bool AcpipIsPmtAvailable;
static bool AcpipPmtUsePort;
static int  AcpipPmtPortNum;
static int* AcpipPmtAddress;

// Setup the ACPI PMT (Power Management Timer)
void AcpiInitPmt()
{
	AcpipIsPmtAvailable = false;
	
	PFADT_HEADER Header = (PFADT_HEADER) HalSdtFacp;
	
	if (Header->PMTimerLength < 4)
	{
	NOT_AVAILABLE:
		SLogMsg("ACPI PMT is not available.");
		return;
	}
	
	if (Header->Header.Revision < 2)
		goto NOT_AVAILABLE;
	
	if (Header->X_PMTimerBlock.Address != 0)
		goto NOT_AVAILABLE;
	
	if (Header->X_PMTimerBlock.AddressSpace == ACPI_ASP_PIO)
	{
		AcpipPmtUsePort = true;
		AcpipPmtPortNum = Header->PMTimerBlock;
		AcpipIsPmtAvailable = true;
		return;
	}
	
	if (Header->X_PMTimerBlock.AddressSpace == ACPI_ASP_PIO)
	{
		AcpipPmtUsePort = false;
		AcpipPmtAddress = (int*) Header->X_PMTimerBlock.Address;
		AcpipIsPmtAvailable = true;
		return;
	}
	
	goto NOT_AVAILABLE;
}

// Note: Pessimistically assume the counter is 24-bit, for simplicity.
int AcpiReadPmtCounter()
{
	if (AcpipPmtUsePort)
	{
		uint32_t Counter = KePortReadDword(AcpipPmtPortNum);
		return Counter & 0xFFFFFF;
	}
	else
	{
		return *AcpipPmtAddress & 0xFFFFFF;
	}
}

void AcpiWritePmtCounter(int Value)
{
	if (AcpipPmtUsePort)
	{
		KePortWriteDword(AcpipPmtPortNum, Value & 0xFFFFFF);
	}
	else
	{
		*AcpipPmtAddress = Value & 0xFFFFFF;
	}
}

void HalAcpiLocateImportantSdts()
{
	int EntryCount = AcpiRsdtGetEntryCount(HalpRsdt);
	
	for (int i = 0; i < EntryCount; i++)
	{
		PSDT_HEADER Header;
		if (AcpiUsingXsdt())
			Header = MmGetHHDMOffsetAddr(HalpRsdt->Sdts64[i]);
		else
			Header = MmGetHHDMOffsetAddr(HalpRsdt->Sdts32[i]);
		
	#ifdef DEBUG
		char Signature[5];
		Signature[4] = 0;
		memcpy(Signature, Header->Signature, 4);
		SLogMsg("ACPI: Found sdt with signature %s", Signature);
	#endif
		
		// Check the signature of this SDT to see if we support it
	#define CHECK(signature, output) \
		do {\
			if (memcmp(Header->Signature, signature, 4) == 0) \
				output = Header; \
		} while (0);
		
		CHECK("HPET", HalSdtHpet);
		CHECK("MCFG", HalSdtMcfg);
		CHECK("FACP", HalSdtFacp);
		CHECK("APIC", HalSdtApic);
	
	#undef CHECK
	}
}

void HalInitAcpi()
{
	if (!KeLimineRsdpRequest.response || !KeLimineRsdpRequest.response->address)
	{
		LogMsg("Warning, ACPI is not supported on this machine.");
		// note: ACPI is necessary for setting up the IOAPIC, I guess we can do without it but uh
		return;
	}
	
	HalpRsdp = KeLimineRsdpRequest.response->address;
	
	if (AcpiUsingXsdt())
		HalpRsdt = MmGetHHDMOffsetAddr(HalpRsdp->XsdtAddress);
	else
		HalpRsdt = MmGetHHDMOffsetAddr(HalpRsdp->RsdtAddress);
	
#ifdef DEBUG
	SLogMsg("ACPI: Revision %d, %susing XSDT, RSDT at %p", HalpRsdp->Revision, AcpiUsingXsdt() ? "" : "not ", HalpRsdt);
#endif
	
	HalAcpiLocateImportantSdts();
	
	AcpiInitPmt();
}

bool HalAcpiIsPmtAvailable()
{
	return AcpipIsPmtAvailable;
}

int HalGetPmtCounter()
{
	return AcpiReadPmtCounter();
}

void HalSetPmtCounter(int Value)
{
	AcpiWritePmtCounter(Value);
}
