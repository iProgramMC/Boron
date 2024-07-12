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

#define PORT_SSTS_DET_DETEST  (0x3)
#define PORT_SSTS_IPM_ACTIVE  (0x1)

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
	PAHCI_MEMORY_REGISTERS ParentHba;
	
	PAHCI_PORT AhciPort;
}
DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct
{
	int Test;
}
FCB_EXTENSION, *PFCB_EXTENSION;

typedef union
{
	struct
	{
		int DeviceDetection : 4;          // DET
		int CurrentInterfaceSpeed : 4;    // SPD
		int InterfacePowerManagement : 4; // IPM
	};
	uint32_t AsUint32;
}
SATA_STATUS, *PSATA_STATUS;

struct _AHCI_PORT
{
	uint32_t CommandListBase;     // CLB
	uint32_t CommandListBaseHigh; // CLBU
	uint32_t FisBaseAddress;      // FB
	uint32_t FisBaseAddressHigh;  // FBU
	uint32_t InterruptStatus;     // IS
	uint32_t InterruptEnable;     // IE
	uint32_t CommandStatus;       // CMD
	uint32_t Reserved0;
	uint32_t TaskFileData;        // TFD
	uint32_t Signature;           // SIG
	SATA_STATUS SataStatus;       // SSTS
	uint32_t SataControl;         // SCTL
	uint32_t SataError;           // SERR
	uint32_t SataActive;          // SACT
	uint32_t CommandIssue;        // CI
	uint32_t SataNotification;    // SNTF
	uint32_t FisSwitchControl;    // FBS
	uint32_t DeviceSleep;         // DEVSLP
	uint8_t  Reserved1[0x70 - 0x48];
	uint8_t  VendorSpecific[0x80 - 0x70];
};

struct _AHCI_GENERIC_HOST_CONTROL
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

struct _AHCI_MEMORY_REGISTERS
{
	AHCI_GENERIC_HOST_CONTROL Ghc;  // 0x00 - 0x2B
	
	uint8_t Reserved0[0x60 - 0x2C]; // 0x2C - 0x5F
	uint8_t Reserved1[0xA0 - 0x60]; // 0x60 - 0x9F -- Reserved for NVMHCI
	uint8_t VendorSpecific[0x100 - 0xA0]; // 0xA0 - 0xFF -- Vendor specific registers
	
	AHCI_PORT PortList[32];
};

extern PDRIVER_OBJECT AhciDriverObject;
extern IO_DISPATCH_TABLE AhciDispatchTable;

bool AhciPciDeviceEnumerated(PPCI_DEVICE Device, void* CallbackContext);
