/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ttyobj.c
	
Abstract:
	This module implements the TTY (Virtual Teletype)
	object.
	
Author:
	iProgramInCpp - 29 October 2025
***/
#include "ttyi.h"

//
// The pseudo-terminal subsystem will be implemented using three
// objects:
//
// 1. The pseudoterminal object itself.
//
// It realizes the connection between the host handle (the terminal
// emulator), and the session handle(s) (the applications running
// through that terminal).  I deemed it necessary to create a whole
// new executive object type and object types because this kind of
// support logic is important to implement in the kernel.
//
// It will be an object manager object of type ``Terminal''.  It
// will not be directly writable using OSWriteFile and OSReadFile
// as it is not a file.
//
// 2. The pseudoterminal "host" file.
//
// This is a regular FCB (that file objects reference), which *can*
// be written to using OSWriteFile and OSReadFile.
//
// 3. The pseudoterminal "session" file.
//
// This is another regular FCB (that different file objects take a
// reference to), which is also writable using the OSWriteFile and
// OSReadFile system services, but it will see more traffic than
// the host side file, as several programs may be writing to it at
// once. (for example, if you run many programs with the `&` flag
// to put them in the background.
//
// Terminals can be created using the OSCreateTerminal system call.
// This will give a handle to the terminal object  (which will be
// distinct from the host and session pseudo-files).
//
// Then, handles to both host and session files can be created with
// the OSCreateIOHandlesTerminal system service, and it creates the
// handles for the host and session side (but they can pass null if 
// they don't care about one of the handles).  Both handles will be
// read/writable using the OSReadFile/OSWriteFile system service.
//

OBJECT_TYPE_INFO TtyTerminalObjectTypeInfo =
{
	.NonPagedPool = false,
	.MaintainHandleCount = false,
	.Delete = TtyDeleteTerminal,
};

void TtyDeleteTerminal(void* ObjectV)
{
	// NOTE: references to the terminal object are held for every
	// file object 
	PTERMINAL Terminal = ObjectV;
	
	ObDereferenceObject(Terminal->HostToSessionPipe);
	ObDereferenceObject(Terminal->SessionToHostPipe);
	IoFreeFcb(Terminal->HostFcb);
	IoFreeFcb(Terminal->SessionFcb);
}

BSTATUS TtyInitializeTerminal(void* TerminalV, void* Context)
{
	BSTATUS Status;
	PTERMINAL Terminal = TerminalV;
	PTERMINAL_INIT_CONTEXT InitContext = Context;
	
	Status = IoCreatePipeObject(&Terminal->HostToSessionPipe, &Terminal->HostToSessionPipeFcb, NULL, InitContext->BufferSize, true);
	if (FAILED(Status))
		return Status;
	
	Status = IoCreatePipeObject(&Terminal->SessionToHostPipe, &Terminal->SessionToHostPipeFcb, NULL, InitContext->BufferSize, true);
	if (FAILED(Status))
		goto Fail1;
	
	Terminal->HostFcb = IoAllocateFcb(&TtyHostDispatch, sizeof(TERMINAL_HOST), false);
	if (!Terminal->HostFcb)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Fail2;
	}
	
	Terminal->SessionFcb = IoAllocateFcb(&TtySessionDispatch, sizeof(TERMINAL_SESSION), false);
	if (!Terminal->SessionFcb)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Fail3;
	}
	
	// Creation succeeded, now initialize everything.
	PTERMINAL_HOST Host = (PTERMINAL_HOST) Terminal->HostFcb->Extension;
	PTERMINAL_HOST Session = (PTERMINAL_HOST) Terminal->SessionFcb->Extension;
	Host->Terminal = Terminal;
	Session->Terminal = Terminal;
	
Fail3:
	IoFreeFcb(Terminal->HostFcb);
Fail2:
	ObDereferenceObject(Terminal->SessionToHostPipe);
	IoFreeFcb(Terminal->HostToSessionPipeFcb);
Fail1:
	ObDereferenceObject(Terminal->HostToSessionPipe);
	IoFreeFcb(Terminal->SessionToHostPipeFcb);
	return Status;
}
