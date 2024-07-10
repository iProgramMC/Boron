/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ahci.h
	
Abstract:
	This header defines data types and includes common system
	headers used by the AHCI storage device driver.
	
Author:
	iProgramInCpp - 7 July 2024
***/
#include <main.h>
#include <ke.h>
#include <io.h>
#include <hal.h>
#include <mm.h>

#define HBA_GHC_AE  (1 << 31)   // Global HBA Control: AHCI Enable

#define HBA_CAPS2_BOH (1 << 0)  // Extended Capabilities: BIOS/OS Handoff

#define HBA_BOHC_BOS  (1 << 0)  // BIOS/OS Handoff Control & Status: BIOS Owned Semaphore
#define HBA_BOHC_OOS  (1 << 1)  // BIOS/OS Handoff Control & Status: OS Owned Semaphore
#define HBA_BOHC_BB   (1 << 4)  // BIOS/OS Handoff Control & Status: BIOS Busy

typedef volatile struct _AHCI_MEMORY_REGISTERS AHCI_MEMORY_REGISTERS, *PAHCI_MEMORY_REGISTERS;
typedef volatile struct _AHCI_GENERIC_HOST_CONTROL AHCI_GENERIC_HOST_CONTROL, *PAHCI_GENERIC_HOST_CONTROL;
typedef volatile struct _AHCI_PORT AHCI_PORT, *PAHCI_PORT;

typedef struct
{
	PPCI_DEVICE PciDevice;
	
	PAHCI_MEMORY_REGISTERS MemRegs;
	
	int NumCommandSlots;
}
CONTROLLER_EXTENSION, *PCONTROLLER_EXTENSION;

typedef struct
{
	int Test;
}
DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct
{
	int Test;
}
FCB_EXTENSION, *PFCB_EXTENSION;

volatile struct _AHCI_PORT
{
	uint32_t CommandListBase;
	uint32_t CommandListBaseHigh;
	uint32_t FisBaseAddress;
	uint32_t FisBaseAddressHigh;
	uint32_t InterruptStatus;
	uint32_t InterruptEnable;
	uint32_t CommandStatus;
	uint32_t Reserved0;
	uint32_t TaskFileData;
	uint32_t Signature;
	uint32_t SataStatus;
	uint32_t SataControl;
	uint32_t SataError;
	uint32_t SataActive;
	uint32_t CommandIssue;
	uint32_t SataNotification;
	uint32_t FisSwitchControl;
	uint32_t DeviceSleep;
	uint8_t  Reserved1[0x70 - 0x48];
	uint8_t  VendorSpecific[0x80 - 0x70];
};

volatile struct _AHCI_GENERIC_HOST_CONTROL
{
	uint32_t HostCapabilities;
	uint32_t GlobalHostControl;
	uint32_t InterruptStatus;
	uint32_t PortsImplemented;
	uint32_t Version;
	uint32_t CccCtl;   // Command Completion Coalescing Control
	uint32_t CccPorts; // Command Completion Coalsecing[sic] Ports
	uint32_t EMLoc;    // Enclosure Management Location
	uint32_t EMCtl;    // Enclosure Management Control
	uint32_t HostCapabilities2;
	uint32_t BiosOsHandOff;
};

static_assert(sizeof(AHCI_PORT) == 0x80);

volatile struct _AHCI_MEMORY_REGISTERS
{
	AHCI_GENERIC_HOST_CONTROL Ghc;  // 0x00 - 0x2B
	
	uint8_t Reserved0[0x60 - 0x2C]; // 0x2C - 0x5F
	uint8_t Reserved1[0xA0 - 0x60]; // 0x60 - 0x9F -- Reserved for NVMHCI
	uint8_t VendorSpecific[0x100 - 0xA0]; // 0xA0 - 0xFF -- Vendor specific registers
	
	AHCI_PORT Ports[32];
};

extern PDRIVER_OBJECT AhciDriverObject;

bool AhciPciDeviceEnumerated(PPCI_DEVICE Device, void* CallbackContext);
