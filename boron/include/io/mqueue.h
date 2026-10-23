/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	io/mqueue.h

Abstract:
	This header defines APIs related to I/O message queue objects.

Author:
	iProgramInCpp - 23 October 2026
***/
#pragma once

#include <ke.h>
#include <ex.h>

// Stream-based communication: messages may be spliced together and partial reads are
// allowed.  This set of behaviors makes the message queue object behave like a FIFO pipe.
//
// The default behavior is discrete packet-based communication.
#define IO_MQ_STREAM (IO_MQ_MULTI_MESSAGE_READ | IO_MQ_TRUNCATE_PARTIAL_READS | IO_MQ_SEGMENT_MESSAGES)

// Allow multiple messages to be read through a single I/O read call.
#define IO_MQ_MULTI_MESSAGE_READ  (1 << 0)

// Drop a message if a read operation could not be completely read into a buffer.
//
// The default behavior is to throw STATUS_BUFFER_OVERFLOW and provide the message's
// size into the IO_STATUS_BLOCK instead.
#define IO_MQ_DROP_PARTIAL_READS (1 << 1)

// Truncate the pending message if a read operation is called with an insufficient buffer
// size.  Implied if IO_MQ_STREAM is set.
#define IO_MQ_TRUNCATE_PARTIAL_READS (1 << 2)

// Stop reading immediately when reading an empty message.  This applies to the normal
// I/O read/write operations only.
#define IO_MQ_STOP_ON_EMPTY_MESSAGES (1 << 3)

// Split a message into multiple segments of MaxMessageLength's size if it is too big to
// fit inside of one message.
//
// The default behavior is to fail with STATUS_BUFFER_OVERFLOW.
//
// WARNING: If a message is being split, the order in which each segment arrives is
// undefined.  If multiple threads are sending messages through the message queue,
// their message segments may be interleaved.
#define IO_MQ_SEGMENT_MESSAGES (1 << 4)

// Block only on the first read - subsequent reads are non-blocking.
//
// The default is to either block for ALL reads, or block for NONE of them.
#define IO_MQ_BLOCK_ONLY_ON_FIRST_READ (1 << 5)

typedef struct
{
	// Start offset into the circular buffer of the parent message queue.
	uint32_t Start;

	// Amount of bytes written -- can be zero.
	uint32_t ByteCount;
}
IO_MESSAGE, *PIO_MESSAGE;

typedef struct
{
	// Maximum bytes on the queue.
	size_t ByteCapacity;

	// Threshold of enqueued bytes for signalling the writer event.
	size_t BytesForWakeUp;

	// Number of enqueued bytes.
	size_t EnqueuedByteCount;

	// Pointer to a byte buffer.
	uint8_t* CircularBuffer;

	// Head of the buffer (ie. next allocation location).
	size_t CircularBufferHead;

	// Flags that determine the operation of the message queue.
	uint32_t Flags;

	// The maximum length of a message.
	uint32_t MaxMessageLength;

	// The maximum amount of messages.
	uint32_t MaxMessageCount;

	// List of messages.
	uint32_t MessageListHead;
	uint32_t MessageListTail;
	uint32_t MessageCount;
	PIO_MESSAGE Messages;

	// Event that is signalled when writers may write to a queue.
	KEVENT WriterEvent;

	// Event that is signalled when readers may read from a queue.
	KEVENT ReaderEvent;

	// A mutex that protects the message queue structure.
	FAST_MUTEX Mutex;

	// The FCB that allows the message queue to be written to or read from
	// with OSReadFile/OSWriteFile.
	PFCB Fcb;
}
IO_MESSAGE_QUEUE, *PIO_MESSAGE_QUEUE;

typedef struct
{
	PIO_MESSAGE_QUEUE MessageQueue;
}
IO_MESSAGE_QUEUE_FCB_EXT, *PIO_MESSAGE_QUEUE_FCB_EXT;

BSTATUS IoCreateMessageQueue(
	PIO_MESSAGE_QUEUE* OutMessageQueue,
	uint32_t Flags,
	uint32_t FcbFileType,
	size_t ByteCapacity,
	size_t BytesForWakeUp,
	uint32_t MaxMessageCount,
	uint32_t MaxMessageLength
);

BSTATUS IoPushMessage(
	PIO_MESSAGE_QUEUE MessageQueue,
	uint8_t* Buffer,
	size_t MessageSize,
	bool NoBlocking,
	bool AlertableWait
);

BSTATUS IoPopMessage(
	PIO_MESSAGE_QUEUE MessageQueue,
	uint8_t* Buffer,
	size_t BufferSize,
	size_t* OutMessageSize,
	bool IsFirstPop,
	bool NoBlocking,
	bool AlertableWait
);

BSTATUS IoPushMessageMdl(
	PIO_MESSAGE_QUEUE MessageQueue,
	PMDL MdlBuffer,
	uintptr_t MdlOffset,
	size_t MdlByteCount,
	bool NoBlocking,
	bool AlertableWait
);

BSTATUS IoPopMessageMdl(
	PIO_MESSAGE_QUEUE MessageQueue,
	PMDL MdlBuffer,
	uintptr_t MdlOffset,
	size_t MdlByteCount,
	size_t* OutMessageSize,
	bool IsFirstPop,
	bool NoBlocking,
	bool AlertableWait
);
