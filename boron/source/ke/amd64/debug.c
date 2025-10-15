/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/amd64/debug.c
	
Abstract:
	This module implements architecture specific debugging
	routines.
	
Author:
	iProgramInCpp - 28 October 2023
***/
#include <ke.h>
#include <arch.h>
#include <ldr.h>
#include <hal.h>
#include <string.h>

#define KERNEL_IMAGE_BASE (0xFFFFFFFF80000000)

#ifdef DEBUG

// if you want stack traces from user mode (NOT recommended for
// release because this is a vulnerability), do this:
#define DISABLE_USER_MODE_PREVENTION

#endif

// Defined in misc.asm:
uintptr_t KiGetRIP();
uintptr_t KiGetRBP();

// Assume that EBP is the first thing pushed when entering a function. This is often
// the case because we specify -fno-omit-frame-pointer when compiling.
// If not, we are in trouble.
typedef struct STACK_FRAME_tag STACK_FRAME, *PSTACK_FRAME;

struct STACK_FRAME_tag
{
	PSTACK_FRAME Next;
	uintptr_t IP;
};

static void DbgResolveAddress(uintptr_t Address, char *SymbolName, size_t BufferSize)
{
	if (!Address)
	{
		// End of our stack trace
		strcpy(SymbolName, "End");
		return;
	}
	
	strcpy(SymbolName, "??");
	
	uintptr_t BaseAddress = 0;
	// Determine where that address came from.
	if (Address >= KERNEL_IMAGE_BASE)
	{
		// Easy, it's in the kernel.
		// Determine the symbol's name
		const char* Name = DbgLookUpRoutineNameByAddress(Address, &BaseAddress);
		
		if (Name)
		{
			snprintf(SymbolName,
					 BufferSize,
					 "brn!%s+%lx",
					 Name,
					 Address - BaseAddress);
		}
		return;
	}
	
	// Determine which loaded DLL includes this address.
	PLOADED_DLL Dll = NULL;
	
	for (int i = 0; i < KeLoadedDLLCount; i++)
	{
		PLOADED_DLL LoadedDll = &KeLoadedDLLs[i];
		if (LoadedDll->ImageBase <= Address && Address < LoadedDll->ImageBase + LoadedDll->ImageSize)
		{
			// It's the one!
			Dll = LoadedDll;
			break;
		}
	}
	
	if (!Dll)
		return;
	
	Address -= Dll->ImageBase;
	
	const char* Name = LdrLookUpRoutineNameByAddress(Dll, Address, &BaseAddress);
	
	if (Name)
	{
		snprintf(SymbolName,
		         BufferSize,
		         "%s!%s+%lx",
		         Dll->Name,
		         Name,
		         Address - BaseAddress);
	}
}

void DbgPrintDouble(const char* String)
{
	DbgPrintString(String);
	HalDisplayString(String);
}

void DbgPrintStackTrace(uintptr_t Rbp)
{
	if (Rbp == 0)
		Rbp = KiGetRBP();
	
	PSTACK_FRAME StackFrame = (PSTACK_FRAME) Rbp;
	
	int Depth = 30;
	char Buffer[128];
	
	DbgPrintDouble("\tAddress         \tName\n");
	
#ifndef DISABLE_USER_MODE_PREVENTION
	if (Rbp <= MM_USER_SPACE_END)
	{
		snprintf(Buffer, sizeof(Buffer), "\t%p\tUser Mode Address\n", (void*) Rbp);
		DbgPrintDouble(Buffer);
		return;
	}
#endif
	
	// TODO: This might be broken and still access a user address. Debug this later
	while (StackFrame && Depth > 0)
	{
		uintptr_t Address = StackFrame->IP;
#ifndef DISABLE_USER_MODE_PREVENTION
		if (Address <= MM_USER_SPACE_END)
		{
			snprintf(Buffer, sizeof(Buffer), "\t%p\tUser Mode Address\n", (void*) Rbp);
			DbgPrintDouble(Buffer);
			return;
		}
#endif
		
		char SymbolName[64];
		DbgResolveAddress(Address, SymbolName, sizeof SymbolName);
		
		snprintf(Buffer, sizeof(Buffer), "\t%p\t%s\n", (void*) Address, SymbolName);
		DbgPrintDouble(Buffer);
		
		Depth--;
		StackFrame = StackFrame->Next;
		
#ifndef DISABLE_USER_MODE_PREVENTION
		if ((uintptr_t)StackFrame <= MM_USER_SPACE_END)
		{
			snprintf(Buffer, sizeof(Buffer), "\t%p\tUser Mode Address\n", (void*) Rbp);
			DbgPrintDouble(Buffer);
			return;
		}
#endif
	}
	
	if (Depth == 0)
		DbgPrintDouble("Warning, stack trace too deep, increase the depth in " __FILE__ " if you need it.\n");
}

#ifdef DEBUG

//#define SERIAL
#ifdef SERIAL

// Serial Port Defines - Copied from NanoShell
//
// We are using COM1 here.
#define PORT_BASE 0x3F8
#define DLAB_ENABLE   (1 << 7)
#define BAUD_DIVISOR  (0x0003)
#define LCR_8BIT_DATA (3 << 0) // Data Bits - 8
#define LCR_PAR_NONE  (0 << 3) // No Parity.
#define FCR_ENABLE    (1 << 0)
#define FCR_RFRES     (1 << 1)
#define FCR_XFRES     (1 << 2)
#define FCR_RXTRIG    (3 << 6)
#define IER_NOINTS    (0)
#define MCR_DTR       (1 << 0)
#define MCR_RTS       (1 << 1)
#define MCR_OUT1      (1 << 2)
#define MCR_LOOPBK    (1 << 4) // loop-back
#define S_RBR 0x00 // Receive buffer register (read only) same as...
#define S_THR 0x00 // Transmitter holding register (write only)
#define S_IER 0x01 // Interrupt enable register
#define S_IIR 0x02 // Interrupt ident register (read only)...
#define S_FCR 0x02 // FIFO control register (write only)
#define S_LCR 0x03 // Line control register
#define S_MCR 0x04 // Modem control register
#define S_LSR 0x05 // Line status register
#define S_MSR 0x06 // Modem status register

#define S_CHECK_BYTE  0xCA

void DbgInit()
{
	KePortWriteByte(PORT_BASE+1, IER_NOINTS);
	
	// Set Divisor to 3 -- 38400 baud
	KePortWriteByte(PORT_BASE+0, BAUD_DIVISOR & 0xFF); 
	KePortWriteByte(PORT_BASE+1, BAUD_DIVISOR  >>  8);
	
	// Set the data parity and bit size. 
	KePortWriteByte(PORT_BASE+3, LCR_8BIT_DATA | LCR_PAR_NONE);
	
	// Enable FIFO
	KePortWriteByte(PORT_BASE+2, FCR_RXTRIG | FCR_XFRES | FCR_RFRES | FCR_ENABLE);
	
	// Prepare modem control register
	KePortWriteByte(PORT_BASE+4, MCR_OUT1 | MCR_RTS | MCR_DTR); // IRQs disabled, RTS/DSR set
	
	// set in loopback mode to test the serial chip
	KePortWriteByte(PORT_BASE+4, MCR_LOOPBK | MCR_OUT1 | MCR_RTS);
	
	// Send a check byte, and check if we get it back
	KePortWriteByte(PORT_BASE+0, S_CHECK_BYTE);
	
	if (KePortReadByte (PORT_BASE + 0) != S_CHECK_BYTE)
	{
		// Hope it still works
	}
	
	// Set this serial PORT_BASE to normal operation
	KePortWriteByte(PORT_BASE+4, MCR_OUT1 | MCR_RTS | MCR_DTR); // IRQs disabled, OUT#1 bit, no loop-back
}

void DbgPrintChar(char c)
{
	while ((KePortReadByte(PORT_BASE + S_LSR) & 0x20) == 0)
		__asm__("pause");
	
	KePortWriteByte(PORT_BASE, c);
}

void DbgPrintString(const char* str)
{
	while (*str)
	{
		if (*str == '\n')
			DbgPrintChar('\r');
		DbgPrintChar(*str);
		str++;
	}
}

#else

void DbgInit()
{
	// E9 port doesn't need initialization.
}

void DbgPrintString(const char* str)
{
	while (*str)
	{
		KePortWriteByte(0xE9, *str);
		str++;
	}
}

#endif

KSPIN_LOCK KiPrintLock;
KSPIN_LOCK KiDebugPrintLock;

void DbgPrintStringLocked(const char* str)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&KiDebugPrintLock, &OldIpl);
	DbgPrintString(str);
	KeReleaseSpinLock(&KiDebugPrintLock, OldIpl);
}

#endif
