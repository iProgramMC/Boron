#pragma once

#include <io.h>
#include <mm.h>
#include <ob.h>
#include <ex.h>
#include <ldr.h>
#include <string.h>

typedef struct
{
	uint8_t BootIndicator;
	uint8_t StartCHS[3]; // unused by Boron
	uint8_t PartTypeDesc;
	uint8_t EndCHS[3];   // unused by Boron
	uint32_t StartLBA;
	uint32_t PartSizeSectors;
}
PACKED
MBR_PARTITION, *PMBR_PARTITION;

// Master Boot Record
typedef struct
{
	uint8_t BootloaderCode [446];
	MBR_PARTITION Partitions[4];
	uint16_t BootSignature; // must be 0xAA55
}
PACKED
MASTER_BOOT_RECORD, *PMASTER_BOOT_RECORD;

#define MBR_BOOT_SIGNATURE 0xAA55

#define IOSB_STATUS(iosb, stat) (iosb->Status = stat)

extern POBJECT_TYPE IoDriverType, IoDeviceType, IoFileType;

// Driver object operations
void IopDeleteDriver(void* Object);

// Device object operations
void IopDeleteDevice(void* Object);
BSTATUS IopParseDevice(void* Object, const char** Name, void* Context, int LoopCount, void** OutObject);

// Controller object operations
void IopDeleteController(void* Object);

// File object operations
BSTATUS IopOpenFile(void* Object, UNUSED int HandleCount, UNUSED OB_OPEN_REASON OpenReason);
void IopDeleteFile(void* Object);
void IopCloseFile(void* Object, int HandleCount);
BSTATUS IopParseFile(void* Object, const char** Name, void* Context, int LoopCount, void** OutObject);
void* IopDuplicateFile(void* Object, int OpenReason);

bool IopInitializeDevicesDir();
bool IopInitializeDriversDir();
bool IopInitializeDeviceType();
bool IopInitializeDriverType();
bool IopInitializeFileType();

// Initializes the partition manager mutexes.
void IopInitPartitionManager();

// Initializes the partition driver object.
void IopInitializePartitionDriverObject();

// Scans for file systems.
void IoScanForFileSystems();

// Create a partition from a block device.
BSTATUS IoCreatePartition(PDEVICE_OBJECT* OutDevice, PDEVICE_OBJECT InDevice, uint64_t Offset, uint64_t Size, size_t Number);
