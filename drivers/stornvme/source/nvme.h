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

#define MAX_IO_QUEUES_PER_CONTROLLER (16)

#define AQA_ADMIN_COMPLETION_QUEUE_SIZE(size) ((size) << 16)
#define AQA_ADMIN_SUBMISSION_QUEUE_SIZE(size)  (size)

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
	IOOP_WRITE = 0x1,
	IOOP_READ  = 0x2,
};

enum // For the Identify.Cns field of the submission queue entry
{
	CNS_NAMESPACE = 0,
	CNS_CONTROLLER,
	CNS_NAMESPACELIST
};

enum // Controller types reported by the NVMe identify command
{
	CT_NONE,
	CT_IO,
	CT_DISCOVERY,
	CT_ADMIN
};

enum // Feature types for NvmeSetFeature
{
	NVME_FEAT_QUEUE_COUNT       = 0x07,
	NVME_FEAT_SOFTWARE_PROGRESS = 0x80,
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
		uint32_t AsUint32;
		struct {
			uint32_t Cns          : 8;
			uint32_t Reserved     : 8;
			uint32_t ControllerId : 16;
		} Identify;
		struct {
			uint32_t FeatureIdentifier : 16;
			uint32_t Reserved          : 15;
			uint32_t Save              : 1;
		} SetFeatures;
		struct {
			uint16_t QueueId;
			uint16_t QueueSize;
		} CreateIoQueue;
		struct {
			uint32_t LbaLow;
		} ReadWrite;
	} Dword10;
	union {
		uint32_t AsUint32;
		struct {
			uint16_t SubQueueCount;
			uint16_t ComQueueCount;
		} SetFeatures;
		struct {
			uint8_t  PhysicallyContiguous : 1;
			uint8_t  QueuePriority        : 2;
			uint16_t Reserved             : 13;
			uint16_t CompletionQueueId    : 16;
		} CreateIoSubQueue;
		struct {
			uint8_t  PhysicallyContiguous : 1;
			uint8_t  InterruptsEnabled    : 1;
			uint16_t Reserved             : 14;
			uint16_t InterruptVector      : 16;
		} CreateIoCompQueue;
		struct {
			uint32_t LbaHigh;
		} ReadWrite;
	} Dword11;
	union {
		uint32_t AsUint32;
		struct {
			uint32_t LogicalBlockCount : 16;
			uint32_t Reserved          : 4;
			uint32_t DirectiveType     : 4; // reserved with IOOP_READ
			uint32_t StorageTagCheck   : 1;
			uint32_t Reserved1         : 1;
			uint32_t ProtectionInfo    : 4;
			uint32_t ForceUnitAccess   : 1;
			uint32_t LimitedRetry      : 1;
		} ReadWrite;
	} Dword12;
	uint32_t CommandDword13;
	uint32_t CommandDword14;
	uint32_t CommandDword15;
}
NVME_SUBMISSION_QUEUE_ENTRY, *PNVME_SUBMISSION_QUEUE_ENTRY;

static_assert(sizeof(NVME_SUBMISSION_QUEUE_ENTRY) == 0x40);

typedef struct
{
	union {
		uint32_t CommandSpecific;
		union {
			struct {
				uint16_t Sub;
				uint16_t Com;
			} QueueCount;
		} SetFeatures;
	};
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
	uint16_t PciVendorId;
	uint16_t PciSubsystemVendorId;
	char     SerialNumber[20];
	char     ModelNumber[40];
	char     FirmwareRevision[8];
	uint8_t  RecommendedArbitrationBurst;
	uint8_t  IeeeOuiIdentifier[3];
	uint8_t  NamespaceSharing;
	uint8_t  MaximumDataTransferSize;
	uint16_t ControllerId;
	uint32_t Version;
	uint32_t Rtd3ResumeLatency;
	uint32_t Rtd3EntryLatency;
	uint32_t OptionalAsyncEventsSupported;
	uint32_t ControllerAttributes;
	uint16_t ReadRecoveryLevelsSupported;
	uint8_t  Reserved0[9];
	uint8_t  ControllerType;
	uint64_t FruGuid[2];
	uint16_t CommandRetryDelay[3];
	uint8_t  Reserved1[240 - 134];
	uint8_t  NvmeMiReserved[253 - 240];
	uint8_t  NvmSubsystemReport;
	uint8_t  VpdWriteCycleInformation;
	uint8_t  ManagementEndpointCapabilities;
	uint16_t OptionalAdminCommandSupport;
	uint8_t  AbortCommandLimit;
	uint8_t  AsynchronousEventRequestLimit;
	uint8_t  FirmwareUpdates;
	uint8_t  LogPageAttributes;
	uint8_t  ErrorLogPageEntries;
	uint8_t  PowerStateSupportCount;
	uint8_t  AdminVendorSpecific;
	uint8_t  Apsta;
	uint16_t Wctemp;
	uint16_t Cctemp;
	uint16_t Mtfa;
	uint32_t HostMemoryBufferPreferredSize;
	uint32_t HostMemoryBufferMinimumSize;
	uint64_t TotalNvmCapacity[2];
	uint64_t UnallocatedNvmCapacity[2];
	uint32_t Rpmbs;
	uint16_t Edstt;
	uint8_t  Dsto;
	uint8_t  Fwug;
	uint16_t Kas;
	uint16_t Hctma;
	uint16_t Mntmt;
	uint16_t Mxtmt;
	uint32_t SaniCap;
	uint32_t Hmminds;
	uint16_t Hmmaxd;
	uint16_t Nsetidmax;
	uint16_t Endgidmax;
	uint8_t  Anatt;
	uint8_t  Anacap;
	uint32_t Anagrpmax;
	uint32_t Nanagrpid;
	uint32_t Pels;
	uint16_t DomainIdentifier;
	uint8_t  Reserved2[512 - 358];
	// NVM Command Set Attributes
	uint8_t  SubQueueEntrySize;
	uint8_t  ComQueueEntrySize;
	uint16_t MaximumOutstandingCommands;
	uint32_t NamespaceCount;
	uint16_t OptionalNvmCommandSupport;
	uint16_t FusedOperationSupport;
	uint8_t  FormatNvmAttributes;
	uint8_t  VolatileWriteCache;
	uint8_t  AtomicWriteUnitNormal;
	uint8_t  AtomicWriteUnitPowerFail;
	uint8_t  IoCommandSetVendorSpecific;
	uint8_t  NamespaceWriteProtection;
	uint8_t  AtomicCompareWriteUnit;
	// More...
}
PACKED
NVME_IDENTIFICATION, *PNVME_IDENTIFICATION;

typedef union
{
	struct {
		uint32_t MetadataSize : 16;
		uint32_t LbaDataSize  : 8;
		uint32_t RelativePerf : 2;
	};
	uint32_t AsUint32;
}
NVME_LBA_FORMAT;

typedef struct
{
	uint64_t NamespaceSize;
	uint64_t NamespaceCapacity;
	uint64_t NamespaceUtilization;
	uint8_t  NamespaceFeatures;
	uint8_t  LbaFormatCount;
	uint8_t  FormattedLbaSize;
	uint8_t  MetadataCapabilities;
	uint8_t  E2eDataProtectionCaps;
	uint8_t  E2eDataProtectionTypeSettings;
	uint8_t  NsMultipathIoAndSharing;
	uint8_t  ReservationCaps;
	uint8_t  FormatProgressIndicator;
	uint8_t  DeallocateLogicalBlockFeatures;
	uint16_t AtomicWriteUnitNormal;
	uint16_t AtomicWriteUnitPowerFail;
	uint16_t AtomicCompareWriteUnit;
	uint16_t AtomicBoundarySizeNormal;
	uint16_t AtomicBoundaryOffset;
	uint16_t AtomicBoundarySizePowerFail;
	uint16_t OptimalIoBoundary;
	uint64_t NvmCapacity[2];
	uint16_t PreferredWriteGranularity;
	uint16_t PreferredWriteAlignment;
	uint16_t PreferredDeallocGranularity;
	uint16_t PreferredDeallocAlignment;
	uint16_t OptimalWriteSize;
	uint16_t MaxSingleSourceRangeLength;
	uint32_t MaximumCopyLength;
	uint8_t  MaximumSourceRangeCount;
	uint8_t  Reserved[92 - 81];
	uint32_t AnaGroupIdentifier;
	uint8_t  Reserved2[99 - 96];
	uint8_t  NamespaceAttributes;
	uint16_t NvmSetIdentifier;
	uint16_t EnduranceGroupIdentifier;
	uint64_t NamespaceGuid[2];
	uint64_t IeeeEuiIdentifier;
	NVME_LBA_FORMAT LbaFormats[64];
}
PACKED
NVME_NAMESPACE_ID, *PNVME_NAMESPACE_ID;

static_assert(sizeof(NVME_NAMESPACE_ID) == 384);

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
	PCONTROLLER_OBJECT ControllerObject;
	PPCI_DEVICE PciDevice;
	
	PNVME_CONTROLLER Controller; // BAR0
	
	PNVME_SUBMISSION_QUEUE_ENTRY SubmissionQueue;
	PNVME_COMPLETION_QUEUE_ENTRY CompletionQueue;
	size_t SubmissionQueueCount;
	size_t CompletionQueueCount;
	size_t DoorbellStride; // NOTE: To get the stride in bytes, do `4 << DoorbellStride`
	size_t MaximumQueueEntries;
	size_t IoQueueCount;
	bool   SoftwareProgressMarkerEnabled;
	size_t ActiveQueueIndex;
	size_t MaximumDataTransferSize;
	
	QUEUE_CONTROL_BLOCK AdminQueue;
	PQUEUE_CONTROL_BLOCK IoQueues;
};

typedef struct
{
	PCONTROLLER_EXTENSION ContExtension;
	
	uint32_t NamespaceId;
	uint64_t Capacity;
	uint16_t BlockSizeLog; // note: 2^BlockSizeLog == BlockSize
	uint16_t BlockSize;
	
	// This is used for paging I/O.  In paging I/O situations, especially write operations,
	// it is forbidden to allocate new memory.
	MMPFN  ReserveIoPagePfn;
	KMUTEX ReserveIoMutex;
}
DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define GET_FCB(FcbExt) (&((PFCB)FcbExt)[-1])
#define GET_DEVICE_OBJECT(DevExt) (&((PDEVICE_OBJECT)DevExt)[-1])
#define GET_CONTROLLER_OBJECT(ContExt) (&((PCONTROLLER_OBJECT)ContExt)[-1])

typedef struct
{
	PCONTROLLER_EXTENSION ControllerExtension;
	PDEVICE_EXTENSION DeviceExtension;
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
BSTATUS NvmeSend(PQUEUE_CONTROL_BLOCK Qcb, PQUEUE_ENTRY_PAIR EntryPair, bool Alertable);

// NOTE: Ownership of the SubmissionQueuePhysical and CompletionQueuePhysical pages is transferred to the queue.
void NvmeSetupQueue(
	PCONTROLLER_EXTENSION ContExtension,
	PQUEUE_CONTROL_BLOCK Qcb,
	uintptr_t SubmissionQueuePhysical,
	uintptr_t CompletionQueuePhysical,
	int DoorBellIndex,
	int MsixIndex,
	size_t SubmissionQueueCount,
	size_t CompletionQueueCount
);

// ==== Commands ====
BSTATUS NvmeIdentify(PCONTROLLER_EXTENSION ContExtension, void* IdentifyBuffer, uint32_t Cns, uint32_t NamespaceId);

BSTATUS NvmeSetFeature(PCONTROLLER_EXTENSION ContExtension, int FeatureIdentifier, uintptr_t DataPointer);

BSTATUS NvmeAllocateIoQueues(PCONTROLLER_EXTENSION ContExtension, size_t QueueCount, size_t* OutQueueCount);

// If "Wait" is set to true, the command will be waited upon.  Otherwise, it will be run asynchronously, and
// it is the caller's duty to have setup a valid event in the queue entry pair and wait on it when done issuing
// requests.
BSTATUS NvmeSendRead(PDEVICE_EXTENSION DeviceExtension, uint64_t Prp[2], uint64_t Lba, uintptr_t BlockCount, bool Wait, PQUEUE_ENTRY_PAIR QueueEntryPtr);

BSTATUS NvmeSendWrite(PDEVICE_EXTENSION DeviceExtension, uint64_t Prp[2], uint64_t Lba, uintptr_t BlockCount, bool Wait, PQUEUE_ENTRY_PAIR QueueEntryPtr);

bool NvmePciDeviceEnumerated(PPCI_DEVICE Device, void* CallbackContext);

// Initializes a queue control block as an I/O queue. The admin queue is initialized in a different place.
BSTATUS NvmeInitializeIoQueue(PCONTROLLER_EXTENSION ContExtension, PQUEUE_CONTROL_BLOCK Qcb, size_t Id);

// ==== I/O Manager Functions ====
BSTATUS NvmeRead(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uintptr_t Offset, PMDL Mdl, uint32_t Flags);
BSTATUS NvmeWrite(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uintptr_t Offset, PMDL Mdl, uint32_t Flags);
size_t  NvmeGetAlignmentInfo(PFCB Fcb);

// ==== Utilities ====
int AllocateVector(PKIPL Ipl, KIPL Default);

FORCE_INLINE volatile uint32_t* GetDoorBell(volatile void* DoorbellArray, int Index, bool IsCompletion, int Stride)
{
	volatile uint8_t* DoorBells = DoorbellArray;
	return (volatile uint32_t*)(DoorBells + (4 << Stride) * (Index * 2 + (IsCompletion ? 1 : 0)));
}

PQUEUE_CONTROL_BLOCK NvmeChooseIoQueue(PCONTROLLER_EXTENSION Extension);
