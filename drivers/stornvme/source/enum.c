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

void NvmeRegisterDevice(PCONTROLLER_OBJECT Controller, const char* ControllerName, int Number)
{
	PCONTROLLER_EXTENSION ContExtension = (PCONTROLLER_EXTENSION) Controller->Extension;
	
	char Buffer[32];
	snprintf(Buffer, sizeof Buffer, "%sDisk%d", ControllerName, Number);
	
	// Create a device object for this device.
	PDEVICE_OBJECT DeviceObject;
	
	BSTATUS Status = IoCreateDevice(
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
		Controller,
		Number,
		DeviceObject
	);
	
	if (FAILED(Status))
		KeCrash("StorNvme: Device #%d was already added to controller", Number);
	
	// Initialize the specific device object.
	
	// TODO
	
	// After the device object was initialized, de-reference it.
	ObDereferenceObject(DeviceObject);
}

BSTATUS NvmeIdentify(PCONTROLLER_EXTENSION ContExtension, void* IdentifyBuffer, uint32_t Cns, uint32_t NamespaceId)
{
	QUEUE_ENTRY_PAIR QueueEntry;
	memset(&QueueEntry, 0, sizeof QueueEntry);
	
	// note: PRP = 0, FusedOperation = 0
	QueueEntry.Sub.CommandHeader.OpCode = ADMOP_IDENTIFY;
	QueueEntry.Sub.NamespaceId = NamespaceId;
	
	// Allocate a memory page to receive identification information.
	int Page = MmAllocatePhysicalPage();
	if (Page == PFN_INVALID)
		return STATUS_INSUFFICIENT_MEMORY;
	
	QueueEntry.Sub.DataPointer[0] = MmPFNToPhysPage(Page);
	
	QueueEntry.Sub.Identify.Cns = Cns;
	
	KEVENT Event;
	KeInitializeEvent(&Event, EVENT_NOTIFICATION, false);
	QueueEntry.Event = &Event;
	
	NvmeSend(&ContExtension->AdminQueue, &QueueEntry);
	
	BSTATUS Status = KeWaitForSingleObject(&Event, false, TIMEOUT_INFINITE);
	if (FAILED(Status))
		return Status;
	
	memcpy(IdentifyBuffer, MmGetHHDMOffsetAddr(MmPFNToPhysPage(Page)), PAGE_SIZE);
	
	MmFreePhysicalPage(Page);
	
	return STATUS_SUCCESS;
}

void Identify(PCONTROLLER_EXTENSION ContExtension)
{
	void* Memory = MmAllocatePool(POOL_PAGED, PAGE_SIZE);
	memset(Memory, 0, PAGE_SIZE);
	
	BSTATUS Status = NvmeIdentify(ContExtension, Memory, CNS_CONTROLLER, 0);
	if (FAILED(Status))
		KeCrash("Stornvme TODO handle identification failure nicely. Status %d", Status);
	
	// hex dump that crap
	uint8_t* Data = Memory;
	LogMsg("Identified:");
	#define A(x) (((x)>=0x20&&(x)<=0x7F)?(x):'.')
	for (size_t i = 0; i < 128; i += 16) {
		LogMsg("%04x: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x    %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c",
			i,
			Data[0], Data[1], Data[2], Data[3], Data[4], Data[5], Data[6], Data[7], 
			Data[8], Data[9], Data[10], Data[11], Data[12], Data[13], Data[14], Data[15],
			A(Data[0]), A(Data[1]), A(Data[2]), A(Data[3]), A(Data[4]), A(Data[5]), A(Data[6]), A(Data[7]), 
			A(Data[8]), A(Data[9]), A(Data[10]), A(Data[11]), A(Data[12]), A(Data[13]), A(Data[14]), A(Data[15])
		);
		Data += 16;
	}
	
	MmFreePool(Memory);
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
	Identify(ContExtension);
	Identify(ContExtension);
	Identify(ContExtension);
	Identify(ContExtension);
	
	// After initializing the controller object, dereference it.
	ObDereferenceObject(ControllerObject);
	return true;
}
 