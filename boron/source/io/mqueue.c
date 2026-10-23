/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	io/mqueue.c

Abstract:
	This module implements the I/O message queue object.

	A message queue object is used for either packet-based or
	stream-based communication between processes.

	The implementation is based on Dennis M. Ritchie's paper called
	"A Stream Input-Output System":
	https://www.nokia.com/bell-labs/about/dennis-m-ritchie/st.pdf

Author:
	iProgramInCpp - 23 October 2026
***/
#include "iop.h"

#define EXT(Fcb) ((PIO_MESSAGE_QUEUE_FCB_EXT)((void*) (Fcb)->Extension))

extern IO_DISPATCH_TABLE IopMessageQueueDispatchTable;

#define MIN_BYTE_CAPACITY   PAGE_SIZE
#define MIN_MESSAGE_COUNT   4
#define MIN_MESSAGE_LENGTH  64

#define WRITERS_INCREMENT 2
#define READERS_INCREMENT 4

//
// Creates a message queue object.  Note: FCB references and dereferences are tracked
// alongside normal object references.
//
// Parameters:
//    OutMessageQueue - The region which will receive the pointer to the newly
//                      created message queue.
//
//    Flags - The flags that control the operation of the message queue. (IO_MQ_*)
//
//    FcbFileType - The file type that the FCB will bear. (can be FILE_TYPE_PIPE,
//                  FILE_TYPE_CHARACTER_DEVICE, FILE_TYPE_SOCKET, etc.)
//
//    ByteCapacity - The maximum amount of bytes that can be stored in this queue
//                   altogether. (sum of every message)
//
//    BytesForWakeUp - The maximum amount of bytes that must be enqueued for writers
//                     to continue being able to write to the queue.
//
//    MaxMessageCount - The maximum amount of messages.
//
//    MaxMessageLength - The maximum length of one message.
//                       Note that ByteCapacity may be reached earlier than when
//                       adding MaxMessageLength * MaxMessageCount bytes.  This is
//                       deliberate.
//
BSTATUS IoCreateMessageQueue(
	PIO_MESSAGE_QUEUE* OutMessageQueue,
	uint32_t Flags,
	uint32_t FcbFileType,
	size_t ByteCapacity,
	size_t BytesForWakeUp,
	uint32_t MaxMessageCount,
	uint32_t MaxMessageLength
)
{
	BSTATUS Status;
	PIO_MESSAGE_QUEUE MessageQueue;
	void* Object;

	// Clean up potentially invalid parameters.
	if (ByteCapacity < MIN_BYTE_CAPACITY) {
		DbgPrint("IoCreateMessageQueue: A ByteCapacity value of %zu is too low.", ByteCapacity);
		ByteCapacity = MIN_BYTE_CAPACITY;
	}

	if (MaxMessageCount < MIN_MESSAGE_COUNT) {
		DbgPrint("IoCreateMessageQueue: A MaxMessageCount value of %u is too low.", MaxMessageCount);
		MaxMessageCount = MIN_MESSAGE_COUNT;
	}

	if (MaxMessageLength < MIN_MESSAGE_LENGTH) {
		DbgPrint("IoCreateMessageQueue: A MaxMessageLength value of %u is too low.", MaxMessageLength);
		MaxMessageLength = MIN_MESSAGE_LENGTH;
	}

	if (ByteCapacity < MaxMessageLength * 2) {
		DbgPrint("IoCreateMessageQueue: A ByteCapacity value of %zu is too low for a MaxMessageLength of %u.", ByteCapacity, MaxMessageLength);
		ByteCapacity = MaxMessageLength * 2;
	}

	if (BytesForWakeUp < MIN_MESSAGE_LENGTH) {
		DbgPrint("IoCreateMessageQueue: A BytesForWakeUp value of %zu is too low.", BytesForWakeUp);
		BytesForWakeUp = MIN_MESSAGE_LENGTH;
	}
	if (BytesForWakeUp > ByteCapacity - MaxMessageLength)
	{
		// TODO: Mitigate this issue properly, rather than circumnavigating it by requiring a
		// low water value that always leaves room for the biggest possible message.
		//
		// TODO: It's possible this actually isn't required at all?
		DbgPrint(
			"IoCreateMessageQueue: BytesForWakeUp is specified to be %zu while byte capacity is "
			"%zu. For optimal performance, ensure that BytesForWakeUp is at most the maximum byte "
			"capacity minus the longest message the pipe expects to be sent.  Otherwise, expect "
			"degraded performance because the writer event will be signalled when it shouldn't.",
			BytesForWakeUp,
			ByteCapacity
		);

		BytesForWakeUp = ByteCapacity - MaxMessageLength;
	}

	if ((Flags & IO_MQ_DROP_PARTIAL_READS) &&
		(Flags & IO_MQ_TRUNCATE_PARTIAL_READS))
	{
		DbgPrint("IoCreateMessageQueue: Cannot drop and truncate at the same time.");
		return STATUS_INVALID_PARAMETER;
	}

	// Create the actual object.
	Status = ObCreateObject(
		&Object,
		NULL,  // ParentDirectory
		IoMessageQueueType,
		NULL,  // ObjectName
		OB_FLAG_NO_DIRECTORY,
		sizeof(IO_MESSAGE_QUEUE)
	);

	if (FAILED(Status)) {
		DbgPrint("IoCreateMessageQueue failed: %s", RtlGetStatusString(Status));
		return Status;
	}

	MessageQueue = Object;

	memset(MessageQueue, 0, sizeof(*MessageQueue));

	// Allocate the circular buffer.
	MessageQueue->CircularBuffer = MmAllocatePool(POOL_PAGED, ByteCapacity);
	if (!MessageQueue->CircularBuffer)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Failure;
	}

	// Allocate the list of messages.
	MessageQueue->Messages = MmAllocatePool(POOL_PAGED, sizeof(IO_MESSAGE) * MaxMessageCount);
	if (!MessageQueue->Messages)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Failure;
	}

	// Allocate the FCB.
	MessageQueue->Fcb = IoAllocateFcb(
		&IopMessageQueueDispatchTable,
		sizeof(IO_MESSAGE_QUEUE_FCB_EXT),
		false
	);

	if (!MessageQueue->Fcb)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Failure;
	}

	// Initialize everything else.
	KeInitializeEvent(&MessageQueue->WriterEvent, EVENT_NOTIFICATION, true);
	KeInitializeEvent(&MessageQueue->ReaderEvent, EVENT_NOTIFICATION, false);
	ExInitializeFastMutex(&MessageQueue->Mutex);

	MessageQueue->ByteCapacity = ByteCapacity;
	MessageQueue->BytesForWakeUp = BytesForWakeUp;
	MessageQueue->EnqueuedByteCount = 0;
	MessageQueue->CircularBufferHead = 0;
	MessageQueue->Flags = Flags;
	MessageQueue->MaxMessageCount = MaxMessageCount;
	MessageQueue->MaxMessageLength = MaxMessageLength;
	MessageQueue->MessageCount = 0;

	MessageQueue->Fcb->FileType = FcbFileType;

	// NOTE: Important, we do NOT add a reference here.
	// Otherwise, this'll be a self-referencing circle and it will never be
	// cleaned up.
	EXT(MessageQueue->Fcb)->MessageQueue = MessageQueue;

	*OutMessageQueue = MessageQueue;
	return Status;

Failure:
	DbgPrint("IoCreateMessageQueue failed: %s", RtlGetStatusString(Status));
	ObDereferenceObject(Object);
	return Status;
}

BSTATUS IopPushMessage(
	PIO_MESSAGE_QUEUE MessageQueue,
	uint8_t* Buffer,
	size_t MessageSize,
	PMDL MdlBuffer,
	uintptr_t MdlOffset,
	bool NoBlocking,
	bool AlertableWait
)
{
	if (MessageSize != 0 && ((Buffer && MdlBuffer) || (Buffer == NULL && MdlBuffer == NULL))) {
		DbgPrint("IoPushMessage: Cannot provide no buffer or both buffers!");
		return STATUS_INVALID_PARAMETER;
	}

	// NOTE: MaxMessageLength isn't expected to be changed mid-operation, so this should
	// be safe to do.
	if (MessageSize > MessageQueue->MaxMessageLength)
	{
		DbgPrint(
			"IoPushMessage: Message of %zu bytes too long (max is %u)",
			MessageSize,
			MessageQueue->MaxMessageLength
		);
		return STATUS_BUFFER_OVERFLOW;
	}

	BSTATUS Status = STATUS_BLOCKING_OPERATION;

	do
	{
		Status = ExAcquireFastMutex(&MessageQueue->Mutex, AlertableWait);
		if (FAILED(Status)) {
			ASSERT(AlertableWait);
			break;
		}

		// Check if we have enough space.
		if (MessageQueue->MessageCount >= MessageQueue->MaxMessageCount ||
			MessageQueue->EnqueuedByteCount + MessageSize >= MessageQueue->ByteCapacity)
		{
			// No, we don't.
			//
			// Set the readers event (in case it wasn't already), reset the writers event
			// so that readers can set it again when writes can happen, and then wait.
			KeResetEvent(&MessageQueue->WriterEvent);
			KeSetEvent(&MessageQueue->ReaderEvent, READERS_INCREMENT);

			if (NoBlocking) {
				DbgPrint("IoPushMessage: Non-blocking I/O requested, and buffer is full, so return immediately");
				ExReleaseFastMutex(&MessageQueue->Mutex);
				Status = STATUS_BLOCKING_OPERATION;
				break;
			}

			ExReleaseFastMutex(&MessageQueue->Mutex);

			Status = KeWaitForSingleObject(
				&MessageQueue->WriterEvent,
				AlertableWait,
				TIMEOUT_INFINITE,
				MODE_KERNEL
			);

			if (FAILED(Status)) {
				break;
			}

			// Give it another try.
			continue;
		}

		// We do have enough space.
		PIO_MESSAGE Message = &MessageQueue->Messages[MessageQueue->MessageListHead++];
		Message->Start = MessageQueue->CircularBufferHead;
		Message->ByteCount = MessageSize;

		if (MessageQueue->MessageListHead >= MessageQueue->MaxMessageCount) {
			MessageQueue->MessageListHead -= MessageQueue->MaxMessageCount;
			ASSERT(MessageQueue->MessageListHead < MessageQueue->MaxMessageCount);
		}

		// Add one to the message count.
		MessageQueue->MessageCount++;
		ASSERT(MessageQueue->MessageCount <= MessageQueue->MaxMessageCount);

		// Advance the circular buffer head.
		MessageQueue->CircularBufferHead += MessageSize;
		MessageQueue->EnqueuedByteCount += MessageSize;

		if (MessageQueue->CircularBufferHead >= MessageQueue->ByteCapacity) {
			MessageQueue->CircularBufferHead -= MessageQueue->ByteCapacity;
			ASSERT(MessageQueue->CircularBufferHead < MessageQueue->ByteCapacity);
		}

		// Now, copy the contents.
		if (Message->Start + MessageSize <= MessageQueue->ByteCapacity)
		{
			if (MdlBuffer) {
				MmCopyFromMdl(MdlBuffer, MdlOffset, &MessageQueue->CircularBuffer[Message->Start], MessageSize);
			}
			else {
				memcpy(&MessageQueue->CircularBuffer[Message->Start], Buffer, MessageSize);
			}
		}
		else
		{
			size_t TailEndSize = MessageQueue->ByteCapacity - Message->Start;
			size_t HeadEndSize = MessageSize - TailEndSize;

			if (MdlBuffer)
			{
				MmCopyFromMdl(MdlBuffer, MdlOffset, &MessageQueue->CircularBuffer[Message->Start], TailEndSize);
				MmCopyFromMdl(MdlBuffer, MdlOffset + TailEndSize, &MessageQueue->CircularBuffer[0], HeadEndSize);
			}
			else
			{
				memcpy(&MessageQueue->CircularBuffer[Message->Start], Buffer, TailEndSize);
				memcpy(&MessageQueue->CircularBuffer[0], Buffer + TailEndSize, HeadEndSize);
			}
		}

		// Signal success, unlock the mutex, and return.
		Status = STATUS_SUCCESS;
		KeSetEvent(&MessageQueue->ReaderEvent, READERS_INCREMENT);
		ExReleaseFastMutex(&MessageQueue->Mutex);
		break;
	}
	while (!NoBlocking);

	return Status;
}

BSTATUS IopPopMessage(
	PIO_MESSAGE_QUEUE MessageQueue,
	uint8_t* Buffer,
	size_t BufferSize,
	PMDL MdlBuffer,
	uintptr_t MdlOffset,
	size_t* OutMessageSize,
	bool IsFirstPop,
	bool NoBlocking,
	bool AlertableWait
)
{
	if ((Buffer && MdlBuffer) || (Buffer == NULL && MdlBuffer == NULL)) {
		DbgPrint("IoPopMessage: Cannot provide no buffer or both buffers!");
		return STATUS_INVALID_PARAMETER;
	}

	if (!BufferSize) {
		DbgPrint("IoPopMessage: Specified a buffer size of 0!?");
		return STATUS_SUCCESS;
	}

	BSTATUS Status = STATUS_BLOCKING_OPERATION;

	bool TruncatePartialReads = MessageQueue->Flags & IO_MQ_TRUNCATE_PARTIAL_READS;
	bool DropPartialReads = MessageQueue->Flags & IO_MQ_DROP_PARTIAL_READS;

	do
	{
		Status = ExAcquireFastMutex(&MessageQueue->Mutex, AlertableWait);
		if (FAILED(Status)) {
			ASSERT(AlertableWait);
			break;
		}

		// Check if there is anything inside.
		if (MessageQueue->MessageCount == 0)
		{
			// No messages pending.  Have to wait.
			//
			// Set the writers event if it wasn't set already, clear the readers event,
			// and wait.
			KeResetEvent(&MessageQueue->ReaderEvent);
			KeSetEvent(&MessageQueue->WriterEvent, WRITERS_INCREMENT);

			if (NoBlocking) {
				DbgPrint("IoPopMessage: Non-blocking I/O requested, and buffer is empty, so return immediately");
				ExReleaseFastMutex(&MessageQueue->Mutex);
				Status = STATUS_BLOCKING_OPERATION;
				break;
			}

			ExReleaseFastMutex(&MessageQueue->Mutex);

			Status = KeWaitForSingleObject(
				&MessageQueue->ReaderEvent,
				AlertableWait,
				TIMEOUT_INFINITE,
				MODE_KERNEL
			);

			if (FAILED(Status)) {
				break;
			}

			// Give it another try.
			continue;
		}

		// There is something inside.
		PIO_MESSAGE Message = &MessageQueue->Messages[MessageQueue->MessageListTail];

		if (Message->ByteCount == 0)
		{
			// Empty message - determine whether we should pop it or not.
			if (!IsFirstPop && (MessageQueue->Flags & IO_MQ_STOP_ON_EMPTY_MESSAGES))
			{
				*OutMessageSize = 0;
				Status = STATUS_END_OF_FILE;
				ExReleaseFastMutex(&MessageQueue->Mutex);
				break;
			}
		}

		// If the buffer size can fit this message, advance the tail.  Otherwise, depends on
		// the specified behavior.
		bool AdvanceTail = false;
		size_t StartPosition = Message->Start;
		size_t ReturnSize = Message->ByteCount;

		ASSERT(Message->Start < MessageQueue->ByteCapacity);

		if (BufferSize < Message->ByteCount)
		{
			if (DropPartialReads)
			{
				// Drop the packet and only return the first "BufferSize" bytes.
				AdvanceTail = true;
				ReturnSize = BufferSize;
				MessageQueue->EnqueuedByteCount -= Message->ByteCount;
			}
			else if (TruncatePartialReads)
			{
				// Truncate the start of the message.
				MessageQueue->EnqueuedByteCount -= BufferSize;
				Message->Start += BufferSize;
				Message->ByteCount -= BufferSize;
				ReturnSize = BufferSize;

				if (Message->Start >= MessageQueue->ByteCapacity) {
					Message->Start -= MessageQueue->ByteCapacity;
					ASSERT(Message->Start < MessageQueue->ByteCapacity);
				}
			}
			else
			{
				// Failure.
				Status = STATUS_BUFFER_OVERFLOW;
				*OutMessageSize = Message->ByteCount;
				ExReleaseFastMutex(&MessageQueue->Mutex);
				break;
			}
		}
		else
		{
			AdvanceTail = true;

			MessageQueue->EnqueuedByteCount -= Message->ByteCount;

#ifdef DEBUG
			Message->Start = 0;
			Message->ByteCount = 0;
#endif
		}

		if (AdvanceTail)
		{
			ASSERT(MessageQueue->MessageCount != 0);
			MessageQueue->MessageListTail++;
			MessageQueue->MessageCount--;
			if (MessageQueue->MessageListTail >= MessageQueue->MaxMessageCount) {
				MessageQueue->MessageListTail -= MessageQueue->MaxMessageCount;
				ASSERT(MessageQueue->MessageListTail < MessageQueue->MaxMessageCount);
			}
		}

		// Now, copy the contents.
		if (StartPosition + ReturnSize <= MessageQueue->ByteCapacity)
		{
			if (MdlBuffer) {
				MmCopyIntoMdl(MdlBuffer, MdlOffset, &MessageQueue->CircularBuffer[StartPosition], ReturnSize);
			}
			else {
				memcpy(Buffer, &MessageQueue->CircularBuffer[StartPosition], ReturnSize);
			}

#ifdef DEBUG
			memset(&MessageQueue->CircularBuffer[StartPosition], 0xCC, ReturnSize);
#endif
		}
		else
		{
			size_t TailEndSize = MessageQueue->ByteCapacity - StartPosition;
			size_t HeadEndSize = ReturnSize - TailEndSize;

			if (MdlBuffer)
			{
				MmCopyIntoMdl(MdlBuffer, MdlOffset, &MessageQueue->CircularBuffer[StartPosition], TailEndSize);
				MmCopyIntoMdl(MdlBuffer, MdlOffset + TailEndSize, &MessageQueue->CircularBuffer[0], HeadEndSize);
			}
			else
			{
				memcpy(Buffer, &MessageQueue->CircularBuffer[StartPosition], TailEndSize);
				memcpy(Buffer + TailEndSize, &MessageQueue->CircularBuffer[0], HeadEndSize);
			}

#ifdef DEBUG
			memset(&MessageQueue->CircularBuffer[StartPosition], 0xCC, TailEndSize);
			memset(&MessageQueue->CircularBuffer[0], 0xCC, HeadEndSize);
#endif
		}

		// Signal success, unlock the mutex, and return.
		Status = STATUS_SUCCESS;
		*OutMessageSize = ReturnSize;

		// If the amount of bytes in the buffer is low enough, signal the writers to wake up.
		if (MessageQueue->EnqueuedByteCount <= MessageQueue->BytesForWakeUp)
		{
			KeSetEvent(&MessageQueue->WriterEvent, WRITERS_INCREMENT);
		}

		ExReleaseFastMutex(&MessageQueue->Mutex);
		break;
	}
	while (!NoBlocking);

	return Status;
}

//
// Add a message to the message queue object.
//
// Parameters:
//    MessageQueue - The affected message queue.
//
//    Buffer - The buffer that contains the message that will be appended to the
//             message queue.
//
//    MessageSize - The size of the message.
//
//    NoBlocking - If the request would block, returns with STATUS_BLOCKING_OPERATION
//                 rather than blocking.
//
//    AlertableWait - Whether the wait on the message queue's mutex can be interrupted.
//
BSTATUS IoPushMessage(
	PIO_MESSAGE_QUEUE MessageQueue,
	uint8_t* Buffer,
	size_t MessageSize,
	bool NoBlocking,
	bool AlertableWait
)
{
	return IopPushMessage(
		MessageQueue,
		Buffer,
		MessageSize,
		NULL,  // MdlBuffer
		0,     // MdlOffset
		NoBlocking,
		AlertableWait
	);
}

//
// Remove a message from the message queue object.
//
// Parameters:
//    MessageQueue - The affected message queue.
//
//    Buffer - The buffer that will receive the message's contents.
//
//    BufferSize - The size of the aforementioned buffer.
//
//    OutMessageSize - The region that will receive the size of the message.
//
//    IsFirstPop - If IO_MQ_STOP_ON_EMPTY_MESSAGES is set, controls whether to pop
//                 an empty message from the queue or not.  If this is the first pop
//                 call during a multi message read, the empty message will be popped.
//                 Otherwise, it will be left in for the next time Read is called.
//
//    NoBlocking - If the request would block, returns with STATUS_BLOCKING_OPERATION
//                 rather than blocking.
//
//    AlertableWait - Whether the wait on the message queue's mutex can be interrupted.
//
BSTATUS IoPopMessage(
	PIO_MESSAGE_QUEUE MessageQueue,
	uint8_t* Buffer,
	size_t BufferSize,
	size_t* OutMessageSize,
	bool IsFirstPop,
	bool NoBlocking,
	bool AlertableWait
)
{
	return IopPopMessage(
		MessageQueue,
		Buffer,
		BufferSize,
		NULL,  // MdlBuffer
		0,     // MdlOffset
		OutMessageSize,
		IsFirstPop,
		NoBlocking,
		AlertableWait
	);
}

//
// Add a message to the message queue object, taking its contents from an MDL.
//
// Parameters:
//    MessageQueue - The affected message queue.
//
//    MdlBuffer - The buffer that contains the message that will be appended to the
//                message queue.
//
//    MdlOffset - The offset into said MDL.
//
//    MdlByteCount - The amount of bytes to cover inside the MDL.
//
//    MessageSize - The size of the message.
//
//    NoBlocking - If the request would block, returns with STATUS_BLOCKING_OPERATION
//                 rather than blocking.
//
//    AlertableWait - Whether the wait on the message queue's mutex can be interrupted.
//
BSTATUS IoPushMessageMdl(
	PIO_MESSAGE_QUEUE MessageQueue,
	PMDL MdlBuffer,
	uintptr_t MdlOffset,
	size_t MdlByteCount,
	bool NoBlocking,
	bool AlertableWait
)
{
	return IopPushMessage(
		MessageQueue,
		NULL,  // Buffer
		MdlByteCount,
		MdlBuffer,
		MdlOffset,
		NoBlocking,
		AlertableWait
	);
}

//
// Remove a message from the message queue object and put its contents in an MDL.
//
// Parameters:
//    MessageQueue - The affected message queue.
//
//    MdlBuffer - The buffer that the received message will be copied into.
//
//    MdlOffset - The offset into the MDL.
//
//    MdlByteCount - The amount of bytes to cover inside the MDL.
//
//    OutMessageSize - The region that will receive the size of the message.
//
//    IsFirstPop - See IoPopMessage.
//
//    NoBlocking - If the request would block, returns with STATUS_BLOCKING_OPERATION
//                 rather than blocking.
//
//    AlertableWait - Whether the wait on the message queue's mutex can be interrupted.
//
BSTATUS IoPopMessageMdl(
	PIO_MESSAGE_QUEUE MessageQueue,
	PMDL MdlBuffer,
	uintptr_t MdlOffset,
	size_t MdlByteCount,
	size_t* OutMessageSize,
	bool IsFirstPop,
	bool NoBlocking,
	bool AlertableWait
)
{
	return IopPopMessage(
		MessageQueue,
		NULL,  // Buffer
		MdlByteCount,
		MdlBuffer,
		MdlOffset,
		OutMessageSize,
		IsFirstPop,
		NoBlocking,
		AlertableWait
	);
}

//
// Destructor function for the message queue object.
//
void IopDeleteMessageQueue(void* Object)
{
	PIO_MESSAGE_QUEUE Queue = Object;

	(void) Queue;
	DbgPrint("IopDeleteMessageQueue");

	if (Queue->CircularBuffer) {
		MmFreePool(Queue->CircularBuffer);
		Queue->CircularBuffer = NULL;
	}

	if (Queue->Messages) {
		MmFreePool(Queue->Messages);
		Queue->Messages = NULL;
	}

	if (Queue->Fcb) {
		IoFreeFcb(Queue->Fcb);
		Queue->Fcb = NULL;
	}
}

void IopReferenceMessageQueueFcb(PFCB Fcb)
{
	ObReferenceObjectByPointer(EXT(Fcb)->MessageQueue);
}

void IopDereferenceMessageQueueFcb(PFCB Fcb)
{
	ObDereferenceObject(EXT(Fcb)->MessageQueue);
}

BSTATUS IopReadMessageQueue(
	PIO_STATUS_BLOCK Iosb,
	PFCB Fcb,
	UNUSED uint64_t Offset,
	PMDL MdlBuffer,
	uint32_t Flags
)
{
	// The FCB need not be locked.  We have our own mutex.
	IoUnlockFcb(Fcb);

	PIO_MESSAGE_QUEUE MessageQueue = EXT(Fcb)->MessageQueue;
	BSTATUS Status = STATUS_SUCCESS;

	ASSERT(MdlBuffer->Flags & MDL_FLAG_WRITE);

	size_t BytesRead = 0;
	Iosb->BytesRead = 0;
	Iosb->Status = Status;

	bool NoBlocking = Flags & IO_RW_NONBLOCK;
	bool BlockOnlyOnFirstRead = MessageQueue->Flags & IO_MQ_BLOCK_ONLY_ON_FIRST_READ;

	if (MessageQueue->Flags & IO_MQ_MULTI_MESSAGE_READ)
	{
		// Continuously read messages until the buffer is full.
		while (BytesRead < MdlBuffer->ByteCount)
		{
			size_t BytesLeft = MdlBuffer->ByteCount - BytesRead;
			size_t MessageSize = 0;
			Status = IoPopMessageMdl(
				MessageQueue,
				MdlBuffer,
				BytesRead, // MdlOffset
				BytesLeft, // MdlByteCount
				&MessageSize,
				BytesRead == 0, // IsFirstPop
				NoBlocking,
				true       // AlertableWait
			);

			if (FAILED(Status))
			{
				if ((!NoBlocking || Status != STATUS_BLOCKING_OPERATION) && Status != STATUS_END_OF_FILE)
				{
					DbgPrint("IopReadMessageQueue: Failed to pop message: %s", RtlGetStatusString(Status));
				}

				if ((Status == STATUS_BLOCKING_OPERATION || Status == STATUS_END_OF_FILE) && BytesRead != 0)
				{
					// Non-block should report success if at least one byte was read
					Status = STATUS_SUCCESS;
				}

				if (Status == STATUS_BUFFER_OVERFLOW)
				{
					if (BytesRead != 0) {
						// Buffer overflow is to only be returned if the buffer was too
						// small for the first message.  (Note: cases where messages are
						// dropped/truncated never return buffer overflow.)
						Status = STATUS_SUCCESS;
					}
					else {
						// There is a buffer overflow, and it will be reported, so put the
						// expected size here.
						//
						// Note that after the 'break', the only piece of code that uses
						// BytesRead is the part that puts it in the IOSB.
						BytesRead = MessageSize;
					}
				}

				break;
			}

			BytesRead += MessageSize;

			if (MessageSize == 0 && (MessageQueue->Flags & IO_MQ_STOP_ON_EMPTY_MESSAGES))
			{
				// Received an empty message, that means it's time to stop.
				break;
			}

			if (BlockOnlyOnFirstRead && MessageSize != 0)
			{
				NoBlocking = true;
			}
		}
	}
	else
	{
		// In packet mode, just read the first packet.
		size_t MessageSize = 0;
		Status = IoPopMessageMdl(
			MessageQueue,
			MdlBuffer,
			0,         // MdlOffset
			MdlBuffer->ByteCount,
			&MessageSize,
			true,      // IsFirstPop
			NoBlocking,
			true       // AlertableWait
		);

		if (SUCCEEDED(Status) || Status == STATUS_BUFFER_OVERFLOW || Status == STATUS_END_OF_FILE) {
			BytesRead = MessageSize;
		}
	}

	Iosb->Status = Status;
	Iosb->BytesRead = BytesRead;

	// Relock the rwlock because the caller expects us to do so.
	IoLockFcbShared(Fcb);
	return Status;
}

BSTATUS IopWriteMessageQueue(
	PIO_STATUS_BLOCK Iosb,
	PFCB Fcb,
	UNUSED uint64_t Offset,
	PMDL MdlBuffer,
	uint32_t Flags
)
{
	// The FCB need not be locked.  We have our own mutex.
	IoUnlockFcb(Fcb);

	PIO_MESSAGE_QUEUE MessageQueue = EXT(Fcb)->MessageQueue;
	BSTATUS Status = STATUS_SUCCESS;

	ASSERT(~MdlBuffer->Flags & MDL_FLAG_WRITE);

	size_t BytesWritten = 0;
	Iosb->BytesWritten = 0;
	Iosb->Status = Status;

	bool NoBlocking = Flags & IO_RW_NONBLOCK;
	bool AllowSegmentation = MessageQueue->Flags & IO_MQ_SEGMENT_MESSAGES;
	size_t MaxMessageLength = MessageQueue->MaxMessageLength;

	if (!AllowSegmentation)
	{
		if (MdlBuffer->ByteCount > MaxMessageLength)
		{
			DbgPrint("IopWriteMessageQueue: Refusing to send because messages are not segmentable.");
			Status = STATUS_BUFFER_OVERFLOW;
			goto Finish;
		}
	}

	// NOTE: This while loop should only ever do one iteration if segmentation is disabled.
	size_t IterationCount = 0;
	while (BytesWritten < MdlBuffer->ByteCount)
	{
		IterationCount++;
		ASSERT(IterationCount == 1 || AllowSegmentation);

		size_t BytesToAdd = MdlBuffer->ByteCount - BytesWritten;
		if (BytesToAdd >= MaxMessageLength)
			BytesToAdd = MaxMessageLength;

		Status = IoPushMessageMdl(
			MessageQueue,
			MdlBuffer,
			BytesWritten,
			BytesToAdd,
			NoBlocking,
			true  // AlertableWait
		);

		if (FAILED(Status)) {
			if (!NoBlocking || Status != STATUS_BLOCKING_OPERATION)
				DbgPrint("IopWriteMessageQueue: Failed to add message: %s", RtlGetStatusString(Status));

			if (Status == STATUS_BLOCKING_OPERATION && BytesWritten != 0) {
				// Non-block should report success if at least one byte was written
				Status = STATUS_SUCCESS;
			}

			break;
		}

		BytesWritten += BytesToAdd;
	}

Finish:
	Iosb->Status = Status;
	Iosb->BytesWritten = BytesWritten;

	// Relock the rwlock because the caller expects us to do so.
	IoLockFcbShared(Fcb);
	return Status;
}

IO_DISPATCH_TABLE IopMessageQueueDispatchTable =
{
	.Reference = IopReferenceMessageQueueFcb,
	.Dereference = IopDereferenceMessageQueueFcb,
	// TODO: iocontrol
	.Read = IopReadMessageQueue,
	.Write = IopWriteMessageQueue
};
