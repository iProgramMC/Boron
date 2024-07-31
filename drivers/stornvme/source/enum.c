/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	enum.c
	
Abstract:
	This module implements PCI device enumeration logic
	for the NVMe disk driver.
	
Author:
	iProgramInCpp - 10 July 2024
***/
#include "nvme.h"
#include <string.h>

// Massive thanks to Mathewnd's Astral for help with this implementation
// https://github.com/Mathewnd/Astral/blob/rewrite/kernel-src/io/block/nvme.c

#define SET_CONFIGURATION(Cfg, Cfg2) ((Cfg).AsUint32 = (Cfg2).AsUint32)
#define SET_CAPABILITIES(Cap, Cap2)  ((Cap).AsUint64 = (Cap2).AsUint64)

int ControllerNumber; // Atomically incremented
int DriveNumber; // TODO: Maybe a system wide solution?

void NvmeInitializeNamespace(PCONTROLLER_EXTENSION ContExtension, uint32_t NamespaceId, const char* ContName)
{
	// Send an identification request.
	void* Memory = MmAllocatePool(POOL_NONPAGED, PAGE_SIZE);
	BSTATUS Status = NvmeIdentify(ContExtension, Memory, CNS_NAMESPACE, NamespaceId);
	if (FAILED(Status))
		KeCrash("StorNvme TODO: failure to identify namespace id 0x%x on controller %s", NamespaceId, ContName);
	
	PNVME_NAMESPACE_ID Ident = Memory;
	
	int BlockSizeLog = Ident->LbaFormats[Ident->FormattedLbaSize & 0xF].LbaDataSize;
	
	// The size of a block should be at most the system page size.
	ASSERT((1 << BlockSizeLog) >= 512 && (1 << BlockSizeLog) <= PAGE_SIZE);
	
	char Buffer[32];
	snprintf(Buffer, sizeof Buffer, "%sDisk%u", ContName, NamespaceId);
	
	// Create a device object for this device.
	PDEVICE_OBJECT DeviceObject;
	
	Status = IoCreateDevice(
		NvmeDriverObject,
		sizeof(DEVICE_EXTENSION),
		sizeof(FCB_EXTENSION),
		Buffer,
		DEVICE_TYPE_BLOCK,
		false,
		&NvmeDispatchTable,
		&DeviceObject
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Cannot create device object called %s: %d", Buffer, Status);
		return;
	}
	
	// Add it to the controller.
	Status = IoAddDeviceController(
		ContExtension->ControllerObject,
		NamespaceId,
		DeviceObject
	);
	
	if (FAILED(Status))
		KeCrash("StorNvme: Device #%u was already added to controller", NamespaceId);
	
	// Initialize the device object.
	PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION) DeviceObject->Extension;
	DeviceExtension->ContExtension = ContExtension;
	DeviceExtension->NamespaceId  = NamespaceId;
	DeviceExtension->Capacity     = Ident->NamespaceCapacity;
	DeviceExtension->BlockSizeLog = BlockSizeLog;
	DeviceExtension->BlockSize    = 1 << BlockSizeLog;
	
	// Create a reserve read/write page to facilitate paging I/O in memory scarce situations.
	DeviceExtension->ReserveReadPagePfn = MmAllocatePhysicalPage();
	ASSERT(DeviceExtension->ReserveReadPagePfn != PFN_INVALID && "TODO: Handle this failure nicely");
	
	KeInitializeMutex(&DeviceExtension->ReserveReadMutex, 1);
	
	// Initialize the FCB extension.
	PFCB_EXTENSION FcbExtension = (PFCB_EXTENSION) DeviceObject->Fcb->Extension;
	FcbExtension->ControllerExtension = ContExtension;
	FcbExtension->DeviceExtension = DeviceExtension;
	
	DbgPrint("StorNvme: %s: %llu blocks with %llu bytes per block", Buffer, Ident->NamespaceCapacity, DeviceExtension->BlockSize);
	
	MmFreePool(Ident);
	
	// After the device object was initialized, de-reference it.
	ObDereferenceObject(DeviceObject);
}

bool NvmePciDeviceEnumerated(PPCI_DEVICE Device, UNUSED void* CallbackContext)
{
	int ContNumber = AtFetchAdd(ControllerNumber, 1);
	char Buffer[16];
	snprintf(Buffer, sizeof Buffer, "Nvme%d", ContNumber);
	PCONTROLLER_OBJECT ControllerObject = NULL;
	BSTATUS Status = IoCreateController(
		NvmeDriverObject,
		sizeof(CONTROLLER_EXTENSION),
		Buffer,
		false,
		&ControllerObject
	);
	
	LogMsg("NvmePciDeviceEnumerated: PCI Ven%04x Dev%04x", Device->Identifier.VendorId, Device->Identifier.DeviceId);
	
	if (FAILED(Status))
	{
		LogMsg("Could not create controller object for %s: status %d", Buffer, Status);
		return false;
	}
	
	PCONTROLLER_EXTENSION ContExtension = (PCONTROLLER_EXTENSION) ControllerObject->Extension;
	
	ContExtension->PciDevice = Device;
	ContExtension->ControllerObject = ControllerObject;
	
	// The base address of the NVMe controller is located at BAR 0.
	// According to the specification, the lower 12 bits are reserved, so they are ignored.
	uintptr_t BaseAddress = HalPciReadBarAddress(&Device->Address, 0);
	
	// Use the HHDM offset to determine the address in virtual memory.
	PNVME_CONTROLLER Controller = MmGetHHDMOffsetAddr(BaseAddress);
	
	ContExtension->Controller = Controller;
	
	// Check if this device supports the NVM command set.
	if (!Controller->Capabilities.Css_NvmCommandSet)
	{
		DbgPrint(
			"StorNvme ERROR: Rejecting device %d:%d:%d because it doesn't support the NVM command set",
			Device->Address.Bus,
			Device->Address.Slot,
			Device->Address.Function
		);
		
	CleanupAndGoAway:
		ObUnlinkObject(ControllerObject);
		ObDereferenceObject(ControllerObject);
		return false;
	}
	
	// Enable interrupts, bus mastering DMA and memory space addressing.
	uint32_t StatusCommand = HalPciConfigReadWord(&Device->Address, PCI_OFFSET_STATUS_COMMAND);
	
	StatusCommand |= PCI_CMD_MEMORYSPACE | PCI_CMD_BUSMASTERING;
	StatusCommand &= ~(PCI_CMD_INTERRUPTDISABLE | PCI_CMD_IOSPACE);
	
	HalPciConfigWriteDword(&Device->Address, PCI_OFFSET_STATUS_COMMAND, StatusCommand);
	
	if (!Device->MsixData.Exists || Device->MsixData.TableSize == 0)
	{
		DbgPrint(
			"StorNvme ERROR: Rejecting device %d:%d:%d because it %s MSI-X and table size is %d",
			Device->Address.Bus,
			Device->Address.Slot,
			Device->Address.Function,
			Device->MsixData.Exists ? "supports" : "doesn't support",
			Device->MsixData.Exists ? Device->MsixData.TableSize : -1
		);
		
		goto CleanupAndGoAway;
	}
	
	NVME_CAPABILITIES Caps;
	NVME_CONFIGURATION Config;
	
	// Disable the controller.
	SET_CONFIGURATION(Config, Controller->Configuration);
	Config.Enable = 0;
	SET_CONFIGURATION(Controller->Configuration, Config);
	
	// Wait for any pending operations to complete.
	while (AtLoad(Controller->Status) & CSTS_RDY)
		KeSpinningHint();
	
	// Read the current configuration register. We are about to update some of its fields to fit our needs.
	SET_CONFIGURATION(Config, Controller->Configuration);
	
	// Clear the "I/O Command Set Selected" to select the NVM command set.
	// Since this controller supports the NVM command set, a value of 0 will select it. 
	Config.IoCommandSet = 0;
	
	// Set the arbitration mechanism to round robin by clearing the AMS.
	Config.ArbitrationMechanism = 0;
	
	// Set the active page size to 4KiB by clearing the MPS.  MPS occupies bits 10:07.
	Config.PageSize = 0;
	
	// Set submission entry size.  Because a submission queue entry is 64 bytes, log2(64) = 6.
	Config.IoSubmissionQueueEntrySizeLog = 6;
	
	// Set completion entry size.  Because a completion queue entry is 16 bytes, log2(16) = 4.
	Config.IoCompletionQueueEntrySizeLog = 4;
	
	// Flush the configuration.
	SET_CONFIGURATION(Controller->Configuration, Config);
	
	// Set up admin queues.
	uint32_t Aqa = 0;
	Aqa |= AQA_ADMIN_SUBMISSION_QUEUE_SIZE(SUBMISSION_QUEUE_SIZE - 1);
	Aqa |= AQA_ADMIN_COMPLETION_QUEUE_SIZE(COMPLETION_QUEUE_SIZE - 1);
	
	AtStore(Controller->AdminQueueAttributes, Aqa);
	
	int PfnSubQueue = MmAllocatePhysicalPage();
	int PfnComQueue = MmAllocatePhysicalPage();
	
	if (PfnSubQueue == PFN_INVALID || PfnComQueue == PFN_INVALID)
	{
		if (PfnSubQueue != PFN_INVALID)
			MmFreePhysicalPage(PfnSubQueue);
		if (PfnComQueue != PFN_INVALID)
			MmFreePhysicalPage(PfnComQueue);
		
		DbgPrint("WARNING: Not enough memory to allocate subqueue/comqueue for NVME device!");
		goto CleanupAndGoAway;
	}
	
	// Store those addresses.
	uint64_t AdminSubmissionQueueBasePhy = MmPFNToPhysPage(PfnSubQueue);
	uint64_t AdminCompletionQueueBasePhy = MmPFNToPhysPage(PfnComQueue);
	
	Controller->AdminSubmissionQueueBase = AdminSubmissionQueueBasePhy;
	Controller->AdminCompletionQueueBase = AdminCompletionQueueBasePhy;
	
	ContExtension->SubmissionQueue = MmGetHHDMOffsetAddr(AdminSubmissionQueueBasePhy);
	ContExtension->CompletionQueue = MmGetHHDMOffsetAddr(AdminCompletionQueueBasePhy);
	ContExtension->SubmissionQueueCount = SUBMISSION_QUEUE_SIZE;
	ContExtension->CompletionQueueCount = COMPLETION_QUEUE_SIZE;
	
	memset(ContExtension->SubmissionQueue, 0, PAGE_SIZE);
	memset(ContExtension->CompletionQueue, 0, PAGE_SIZE);
	
	// Enable the controller and wait for it to finish.
	SET_CONFIGURATION(Config, Controller->Configuration);
	Config.Enable = 1;
	SET_CONFIGURATION(Controller->Configuration, Config);
	
	uint32_t StatusReg;
	while (!((StatusReg = AtLoad(Controller->Status)) & (CSTS_RDY | CSTS_CFS)))
		KeSpinningHint();
	
	if (StatusReg & CSTS_CFS)
	{
		DbgPrint("ERROR: NVME controller returned fatal during initialization!");
		goto CleanupAndGoAway;
	}
	
	SET_CAPABILITIES(Caps, Controller->Capabilities);
	
	// Record device capabilities inside the controller extension.
	ContExtension->DoorbellStride = Caps.DoorbellStride;
	ContExtension->MaximumQueueEntries = Caps.MaxQueueEntriesSupported + 1;
	
	// Initialize the admin queue.
	NvmeSetupQueue(
		ContExtension,
		&ContExtension->AdminQueue,
		AdminSubmissionQueueBasePhy,
		AdminCompletionQueueBasePhy,
		0,
		0
	);
	
	// Send an identification request.
	PNVME_IDENTIFICATION Ident = MmAllocatePool(POOL_PAGED, PAGE_SIZE);
	memset(Ident, 0, PAGE_SIZE);
	
	Status = NvmeIdentify(ContExtension, Ident, CNS_CONTROLLER, 0);
	if (FAILED(Status))
		KeCrash("Stornvme TODO handle identification failure nicely. Status %d", Status);
	
	if (Ident->ControllerType != 0 && Ident->ControllerType != CT_IO)
		// Here we would need to free the queue, deinitialize the controller and return
		KeCrash("Stornvme TODO handle non-I/O controllers nicely.");
	
	// Enable the software progress marker feature, if available
	Status = NvmeSetFeature(ContExtension, NVME_FEAT_SOFTWARE_PROGRESS, 0);
	if (FAILED(Status) && Status != STATUS_HARDWARE_IO_ERROR)
		KeCrash("Stornvme TODO handle set feature failure nicely. Status %d", Status);
	
	ContExtension->SoftwareProgressMarkerEnabled = SUCCEEDED(Status);
	
	// Get namespace list.
	uint32_t* NamespaceList = MmAllocatePool(POOL_PAGED, PAGE_SIZE);
	memset(NamespaceList, 0, sizeof NamespaceList);
	
	Status = NvmeIdentify(ContExtension, NamespaceList, CNS_NAMESPACELIST, 0);
	if (FAILED(Status))
		KeCrash("Stornvme TODO handle failure to enumerate namespace list nicely. Status %d", Status);
	
	// Determine how many pairs to use.
	size_t IoMin = Device->MsixData.TableSize;
	if (IoMin > MAX_IO_QUEUES_PER_CONTROLLER)
		IoMin = MAX_IO_QUEUES_PER_CONTROLLER;
	ASSERT(IoMin != 0);
	
	size_t IoQueueCount = 0;
	Status = NvmeAllocateIoQueues(ContExtension, IoMin, &IoQueueCount);
	if (FAILED(Status))
		KeCrash("Stornvme TODO handle failure to allocate I/O queues nicely. Status %d", Status);
	
	ASSERT(IoQueueCount != 0);
	if (IoQueueCount > IoMin)
		IoQueueCount = IoMin;
	
	ContExtension->IoQueueCount = IoQueueCount;
	
	size_t AllocSize = sizeof(QUEUE_CONTROL_BLOCK) * IoQueueCount;
	ContExtension->IoQueues = MmAllocatePool(POOL_NONPAGED, AllocSize);
	
	DbgPrint("StorNvme: %s Using %zu I/O queues", Buffer, ContExtension->IoQueueCount);
	
	// Create the I/O queues.
	for (size_t i = 0; i < IoQueueCount; i++)
	{
		Status = NvmeInitializeIoQueue(ContExtension, &ContExtension->IoQueues[i], i + 1);
		ASSERT(Status == STATUS_SUCCESS);
	}
	
	// Initialize namespaces.
	for (size_t i = 0; i < 1024 && NamespaceList[i]; i++)
	{
		NvmeInitializeNamespace(ContExtension, NamespaceList[i], Buffer);
	}
	
	MmFreePool(NamespaceList);
	MmFreePool(Ident);
	
	// After initializing the controller object, dereference it.
	ObDereferenceObject(ControllerObject);
	return true;
}
 