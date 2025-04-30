/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	commands.c
	
Abstract:
	This module implements the I/O manager facing read/write
	functions for the NVMe disk driver.
	
Author:
	iProgramInCpp - 28 July 2024
***/
#include "nvme.h"
#include <string.h>

#define IO_STATUS(Iosb, Stat) ((Iosb)->Status = (Stat))

BSTATUS NvmePerformIoOperation(
	PIO_STATUS_BLOCK Iosb,
	PFCB Fcb,
	uint64_t Lba,
	uint64_t BlockCount,
	PMDL Mdl,
	bool IsWrite,
	size_t PageOffsetInMdl,
	size_t MdlSizeOverride
)
{
	DbgPrint("NvmePerformIoOperation(%p, %llu, %zu, %zu)", Lba, BlockCount, PageOffsetInMdl, MdlSizeOverride);
	BSTATUS Status = STATUS_SUCCESS;
	QUEUE_ENTRY_PAIR Qep;
	
	size_t PageCount = Mdl->NumberPages;
	
	// TODO: The IO system needs to restrict this I believe
	ASSERT(PageCount <= 512);
	
	if (MdlSizeOverride != 0)
		PageCount = MdlSizeOverride;
	
	PFCB_EXTENSION FcbExtension = (PFCB_EXTENSION) Fcb->Extension;
	PDEVICE_EXTENSION DeviceExtension = FcbExtension->DeviceExtension;
	
	int BlockSizeLog = DeviceExtension->BlockSizeLog;
	int PrpPfn = PFN_INVALID;
	
	uint64_t Prp[2];
	
	Prp[0] = MmPFNToPhysPage(Mdl->Pages[PageOffsetInMdl]) + Mdl->ByteOffset;
	
	if (PageCount > 2)
	{
		// We will need to allocate an extra page for the PRP list, as the PRP list only takes two pages
		PrpPfn = MmAllocatePhysicalPage();
		if (PrpPfn == PFN_INVALID)
		{
			// Invalid, so we may need to split this write up into parts.
			// TODO: Test me!
			// TESTME
			for (size_t i = 0; i < PageCount; i++)
			{
				size_t BlocksPerPage = PAGE_SIZE >> BlockSizeLog;
				BSTATUS Status = NvmePerformIoOperation(
					Iosb,
					Fcb,
					Lba + BlocksPerPage * i,
					BlocksPerPage,
					Mdl,
					IsWrite,
					i,
					Mdl->ByteOffset == 0 ? 1 : 2
				);
				
				if (FAILED(Status))
					return Status;
			}
			
			return IO_STATUS(Iosb, STATUS_SUCCESS);
		}
		
		// Copy the rest of the pages into the PRP.
		uint64_t* PrpListAddr = MmGetHHDMOffsetAddr(MmPFNToPhysPage(PrpPfn));
		for (size_t Index = 1; Index < PageCount; Index++)
		{
			PrpListAddr[Index - 1] = MmPFNToPhysPage(Mdl->Pages[PageOffsetInMdl + Index]);
		}
		
		Prp[1] = MmPFNToPhysPage(PrpPfn);
	}
	else if (PageCount == 2)
	{
		Prp[1] = MmPFNToPhysPage(Mdl->Pages[PageOffsetInMdl + 1]);
	}
	else
	{
		Prp[1] = 0;
	}
	
	// Finally, send the read/write command.
	if (IsWrite)
		Status = NvmeSendWrite(DeviceExtension, Prp, Lba, BlockCount, true, &Qep);
	else
		Status = NvmeSendRead(DeviceExtension, Prp, Lba, BlockCount, true, &Qep);
	
	// If we had to allocate a PRP list, then free it.
	if (PrpPfn != PFN_INVALID)
		MmFreePhysicalPage(PrpPfn);
	
	return IO_STATUS(Iosb, Status);
}

// NOTE for reading and writing:
//
// - If reading 1 page, DataPointer[0] is used and point to physical pages which are written to by the controller
//
// - If reading 2 pages, DataPointer[0] and DataPointer[1] are used and ditto
//
// - If reading more than 2 pages' worth of blocks, DataPointer[0] is used and points to
//   a physical page written to by the controller, and DataPointer[1] points to a physical
//   page which contains a list of physical pages written to by the controller.

BSTATUS NvmeRead(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL Mdl, uint32_t Flags)
{
	ASSERT(Mdl->Flags & MDL_FLAG_WRITE);
	
	size_t Length = Mdl->ByteCount;
	
	PFCB_EXTENSION FcbExtension = (PFCB_EXTENSION) Fcb->Extension;
	PDEVICE_EXTENSION DeviceExtension = FcbExtension->DeviceExtension;
	
	int BlockSizeLog = DeviceExtension->BlockSizeLog;
	int BlockSize    = DeviceExtension->BlockSize;
	uint64_t Mask    = BlockSize - 1;
	
	// If the offset or the length are unaligned, then return out an unaligned I/O attempt error.
	if ((Offset & Mask) || (Length & Mask))
		return Iosb->Status = STATUS_UNALIGNED_OPERATION;
	
	uint64_t Lba = Offset >> BlockSizeLog;
	uint64_t BlockCount = Length >> BlockSizeLog;
	
	if (!BlockCount)
		// No I/O to be done actually
		return IO_STATUS(Iosb, STATUS_SUCCESS);
	
	// The NVMe driver does not take into account non-block read/write operations, because the NVMe controller
	// cannot instantly reply to requests from us.
	if (Flags & IO_RW_NONBLOCK)
		return IO_STATUS(Iosb, STATUS_INVALID_PARAMETER);
	
	return NvmePerformIoOperation(Iosb, Fcb, Lba, BlockCount, Mdl, false, 0, 0);
}

BSTATUS NvmeWrite(PIO_STATUS_BLOCK Iosb, UNUSED PFCB Fcb, UNUSED uint64_t Offset, UNUSED PMDL Mdl, UNUSED uint32_t Flags)
{
	ASSERT(!"Are you actually writing to NVMe volumes? Remove this!");
	ASSERT(~Mdl->Flags & MDL_FLAG_WRITE);
	
	size_t Length = Mdl->ByteCount;
	
	PFCB_EXTENSION FcbExtension = (PFCB_EXTENSION) Fcb->Extension;
	PDEVICE_EXTENSION DeviceExtension = FcbExtension->DeviceExtension;
	
	int BlockSizeLog = DeviceExtension->BlockSizeLog;
	int BlockSize    = DeviceExtension->BlockSize;
	uint64_t Mask    = BlockSize - 1;
	
	// If the offset or the length are unaligned, then return out an unaligned I/O attempt error.
	if ((Offset & Mask) || (Length & Mask))
		return Iosb->Status = STATUS_UNALIGNED_OPERATION;
	
	uint64_t Lba = Offset >> BlockSizeLog;
	uint64_t BlockCount = Length >> BlockSizeLog;
	
	if (!BlockCount)
		// No I/O to be done actually
		return IO_STATUS(Iosb, STATUS_SUCCESS);
	
	// The NVMe driver does not take into account non-block read/write operations, because the NVMe controller
	// cannot instantly reply to requests from us.
	if (Flags & IO_RW_NONBLOCK)
		return IO_STATUS(Iosb, STATUS_INVALID_PARAMETER);
	
	return NvmePerformIoOperation(Iosb, Fcb, Lba, BlockCount, Mdl, true, 0, 0);
}

size_t NvmeGetAlignmentInfo(PFCB Fcb)
{
	PFCB_EXTENSION FcbExtension = (PFCB_EXTENSION) Fcb->Extension;
	return 1 << FcbExtension->DeviceExtension->BlockSizeLog;
}
