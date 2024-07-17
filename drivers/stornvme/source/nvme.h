/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	nvme.h
	
Abstract:
	This header defines data types and includes common system
	headers used by the NVMe storage device driver.
	
Author:
	iProgramInCpp - 7 July 2024
***/
#include <main.h>
#include <ke.h>
#include <io.h>
#include <hal.h>
#include <mm.h>

#define CSTS_RDY   (1 << 0) // Ready
#define CSTS_CFS   (1 << 1) // Controller Fatal Status
#define CSTS_PP    (1 << 5) // Processing Paused

#define CC_EN      (1 << 0) // Enable

#define AQA_ADMIN_COMPLETION_QUEUE_SIZE(size) ((size) << 16)
#define AQA_ADMIN_SUBMISSION_QUEUE_SIZE(size)  (size)

typedef union
{
	struct {
		int MaxQueueEntriesSupported        : 16;// 15:00
		int ContiguousQueuesRequired        : 1; // 16
		int ArbitrationMechanismSupported   : 2; // 18:17
		int Reserved0                       : 5; // 23:19
		int Timeout                         : 8; // 31:24
		int DoorbellStride                  : 4; // 35:32
		int NvmSubsystemResetSupported      : 1; // 36
		int   Css_NvmCommandSet             : 1; // 37
		int   Css_Reserved                  : 5; // 42:38
		int   Css_IoCommandSetSupported     : 1; // 43
		int   Css_AdminCommandSetOnly       : 1; // 44
		int BootPartitionSupport            : 1; // 45
		int ControllerPowerScope            : 2; // 47:46
		int MemoryPageSizeMinimum           : 4; // 51:48
		int MemoryPageSizeMaximum           : 4; // 55:52
		int PersistentMemoryRegionSupported : 1; // 56
		int ControllerMemoryBufferSupported : 1; // 57
		int NvmSubsystemShutdownSupported   : 1; // 58
		int ControllerReadyModesSupported   : 2; // 60:59
		int Reserved1                       : 3; // 63:61
	};
	
	uint64_t AsUint64;
}
NVME_CAPABILITIES;

static_assert(sizeof(NVME_CAPABILITIES) == 8);

typedef struct
{
	uint32_t CommandDword0;
	uint32_t NameSpaceId;
	uint64_t Reserved;
	uint64_t MetadataPointer;
	uint64_t DataPointerPrp0;
	uint64_t DataPointerPrp1;
	uint32_t CommandSpecific[6];
}
NVME_SUBMISSION_QUEUE_ENTRY, *PNVME_SUBMISSION_QUEUE_ENTRY;

static_assert(sizeof(NVME_SUBMISSION_QUEUE_ENTRY) == 0x40);

typedef struct
{
	uint32_t CommandSpecific;
	uint32_t Reserved;
	uint16_t SubmissionQueueHeadPointer;
	uint16_t SubmissionQueueIdentifier;
	uint16_t CommandIdentifier;
	
	struct {
		bool    Phase : 1;
		int16_t Code  : 15;
	}
	Status;
}
NVME_COMPLETION_QUEUE_ENTRY, *PNVME_COMPLETION_QUEUE_ENTRY;

static_assert(sizeof(NVME_COMPLETION_QUEUE_ENTRY) == 0x10);

typedef union
{
	struct {
		int Enable       : 1;
		int Reserved     : 3;
		int IoCommandSet : 3;
		int PageSize     : 4;
		int ArbitrationMechanism : 3;
		int ShutdownNotification : 2;
		int IoSubmissionQueueEntrySizeLog : 4;
		int IoCompletionQueueEntrySizeLog : 4;
		int Crime : 1; // Controller Ready Independent Of Media Enable
		int Reserved1 : 7;
	};
	
	uint32_t AsUint32;
}
NVME_CONFIGURATION, *PNVME_CONFIGURATION;

static_assert(sizeof(NVME_CONFIGURATION) == sizeof(uint32_t));

typedef volatile struct _NVME_CONTROLLER
{
	NVME_CAPABILITIES Capabilities;    // 00h - CAP
	uint32_t Version;                  // 08h - VS
	uint32_t InterruptMaskSet;         // 0Ch - INTMS
	uint32_t InterruptMaskClear;       // 10h - INTMC
	NVME_CONFIGURATION Configuration;  // 14h - CC
	uint32_t Reserved0;                // 18h
	uint32_t Status;                   // 1Ch - CSTS
	uint32_t NvmSubsystemReset;        // 20h - NSSR
	uint32_t AdminQueueAttributes;     // 24h - AQA
	uint64_t AdminSubmissionQueueBase; // 28h - ASQ
	uint64_t AdminCompletionQueueBase; // 30h - ACQ
	uint32_t MemoryBufferLocation;     // 38h - CMBLOC
	uint32_t MemoryBufferSize;         // 3Ch - CMBSZ
	uint32_t BootPartitionInfo;        // 40h - BPINFO
	uint32_t BootPartitionReadSelect;  // 44h - BPRSEL
	uint64_t BootPartMemoryBufferLoc;  // 48h - BPMBL
	uint64_t MemoryBufferMemSpcCtl;    // 50h - CMBMSC
	uint32_t MemoryBufferStatus;       // 58h - CMBSTS
	uint32_t MemoryBufferElasticBufSiz;// 5Ch - CMBEBS
	uint32_t MemoryBufferSustWrThruput;// 60h - CMBSWTP
	uint32_t NvmSubsystemShutdown;     // 64h - NSSD
	uint32_t ReadyTimeOuts;            // 68h - CRTO
	uint32_t Reserved1;                // 6Ch
	uint8_t  Padding[0x1000 - 0x70];   // 70h
	uint8_t  DoorBells[];              // 1000h - Doorbell stride is specified in Capabilities.DoorbellStride
}
NVME_CONTROLLER, *PNVME_CONTROLLER;

static_assert(sizeof(NVME_CONTROLLER) == 0x1000);

typedef struct
{
	PPCI_DEVICE PciDevice;
	
	PNVME_CONTROLLER Controller;
	
	PNVME_SUBMISSION_QUEUE_ENTRY SubmissionQueue;
	PNVME_COMPLETION_QUEUE_ENTRY CompletionQueue;
	size_t SubmissionQueueCount;
	size_t CompletionQueueCount;
	size_t DoorbellStride;
	size_t MaximumQueueEntries;
	
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

extern PDRIVER_OBJECT NvmeDriverObject;
extern IO_DISPATCH_TABLE NvmeDispatchTable;

bool NvmePciDeviceEnumerated(PPCI_DEVICE Device, void* CallbackContext);
