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

// Priority boost used when setting events or releasing semaphores.
#define NVME_PRIORITY_BOOST 1

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

enum
{
	ADMOP_DELETE_IO_SUBMISSION_QUEUE = 0x0,
	ADMOP_CREATE_IO_SUBMISSION_QUEUE = 0x1,
	ADMOP_DELETE_IO_COMPLETION_QUEUE = 0x4,
	ADMOP_CREATE_IO_COMPLETION_QUEUE = 0x5,
	ADMOP_IDENTIFY                   = 0x6,
	ADMOP_SET_FEATURES               = 0x9,
	ADMOP_GET_FEATURES               = 0xA,
};

enum
{
	CNS_NAMESPACE = 0,
	CNS_CONTROLLER,
	CNS_NAMESPACELIST
};

typedef union
{
	struct {
		int OpCode            : 8;
		int FusedOperation    : 2;  // 0 - Normal, 1 - Fused first command, 2 - Fused second command
		int Reserved          : 4;
		int PrpSgl            : 2;  // 0 - PRP, 1 - SGL + single contiguous physical buffer, 2 - SGL + address of SGL segment
		int CommandIdentifier : 16; // N.B. Don't use 0xFFFF
	};
	
	uint32_t AsUint32;
}
NVME_COMMAND_HEADER, *PNVME_COMMAND_HEADER;

static_assert(sizeof(NVME_COMMAND_HEADER) == 0x4);

typedef struct
{
	NVME_COMMAND_HEADER CommandHeader; // Command Dword 0
	uint32_t NamespaceId;
	uint32_t CommandDword2;
	uint32_t CommandDword3;
	uint64_t MetadataPointer;
	uint64_t DataPointer[2];
	
	union {
		uint32_t CommandDword10;
		struct {
			uint32_t Cns          : 8;
			uint32_t Reserved     : 8;
			uint32_t ControllerId : 16;
		} Identify;
	};
	uint32_t CommandDword11;
	uint32_t CommandDword12;
	uint32_t CommandDword13;
	uint32_t CommandDword14;
	uint32_t CommandDword15;
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

#define SUBMISSION_QUEUE_SIZE (PAGE_SIZE / sizeof(NVME_SUBMISSION_QUEUE_ENTRY))
#define COMPLETION_QUEUE_SIZE (PAGE_SIZE / sizeof(NVME_COMPLETION_QUEUE_ENTRY))

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
	NVME_SUBMISSION_QUEUE_ENTRY Sub;
	NVME_COMPLETION_QUEUE_ENTRY Comp;
	PKEVENT Event;
}
QUEUE_ENTRY_PAIR, *PQUEUE_ENTRY_PAIR;

typedef struct
{
	void*  Address;
	size_t EntryCount;
	int    Index;
	int    Phase;
	volatile uint32_t* DoorBell;
}
QUEUE_ACCESS_BLOCK, *PQUEUE_ACCESS_BLOCK;

typedef struct _CONTROLLER_EXTENSION CONTROLLER_EXTENSION, *PCONTROLLER_EXTENSION;

typedef struct
{
	PCONTROLLER_EXTENSION Controller;
	KDPC Dpc;
	KSEMAPHORE Semaphore;
	KSPIN_LOCK SpinLock;
	KSPIN_LOCK InterruptSpinLock; // Not actually used for anything
	KINTERRUPT Interrupt;
	QUEUE_ACCESS_BLOCK SubmissionQueue;
	QUEUE_ACCESS_BLOCK CompletionQueue;
	uintptr_t SubmissionQueuePhysical;
	uintptr_t CompletionQueuePhysical;
	PQUEUE_ENTRY_PAIR Entries[PAGE_SIZE / sizeof(NVME_SUBMISSION_QUEUE_ENTRY)];
}
QUEUE_CONTROL_BLOCK, *PQUEUE_CONTROL_BLOCK;

struct _CONTROLLER_EXTENSION
{
	PPCI_DEVICE PciDevice;
	
	PNVME_CONTROLLER Controller; // BAR0
	
	PNVME_SUBMISSION_QUEUE_ENTRY SubmissionQueue;
	PNVME_COMPLETION_QUEUE_ENTRY CompletionQueue;
	size_t SubmissionQueueCount;
	size_t CompletionQueueCount;
	size_t DoorbellStride; // NOTE: To get the stride in bytes, do `4 << DoorbellStride`
	size_t MaximumQueueEntries;
	
	QUEUE_CONTROL_BLOCK AdminQueue;
	PQUEUE_CONTROL_BLOCK IoQueues;
};

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

// ==== Queue Management ====

// Send a raw command to a queue. Note that the PKEVENT Event field of EntryPair
// must point to a valid event. The event may be initialized as a synchronization
// event if reuse is desired.
// 
// After sending, one may wait on the event passed into EntryPair. Once the operation is
// complete, the event will be set.
void NvmeSend(PQUEUE_CONTROL_BLOCK Qcb, PQUEUE_ENTRY_PAIR EntryPair);

// NOTE: Ownership of the SubmissionQueuePhysical and CompletionQueuePhysical pages is transferred to the queue.
void NvmeSetupQueue(
	PCONTROLLER_EXTENSION ContExtension,
	PQUEUE_CONTROL_BLOCK Qcb,
	uintptr_t SubmissionQueuePhysical,
	uintptr_t CompletionQueuePhysical,
	int DoorBellIndex,
	int MsixIndex
);


bool NvmePciDeviceEnumerated(PPCI_DEVICE Device, void* CallbackContext);

// Utilities
int AllocateVector(PKIPL Ipl, KIPL Default);

FORCE_INLINE volatile uint32_t* GetDoorBell(volatile void* DoorbellArray, int Index, bool IsCompletion, int Stride)
{
	volatile uint8_t* DoorBells = DoorbellArray;
	return (volatile uint32_t*)(DoorBells + (4 << Stride) * (Index * 2 + (IsCompletion ? 1 : 0)));
}
