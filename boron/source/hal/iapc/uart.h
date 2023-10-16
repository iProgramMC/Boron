/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/uart.c
	
Abstract:
	This header file contains definitions purporting to
	the UART controller on the AMD64 platform.
	
Author:
	iProgramInCpp - 15 October 2023
***/
#ifndef BORON_HAL_IAPC_UART_H
#define BORON_HAL_IAPC_UART_H

// Setting the baud rate
#define DLAB_ENABLE    (1 << 7)
#define BAUD_DIVISOR   (0x0003)

// Line Control register
#define LCR_5BIT_DATA (0 << 0) // Data Bits - 5
#define LCR_6BIT_DATA (1 << 0) // Data Bits - 6
#define LCR_7BIT_DATA (2 << 0) // Data Bits - 7
#define LCR_8BIT_DATA (3 << 0) // Data Bits - 8
#define LCR_PAR_SPACE (7 << 3) // SPACE Parity.
#define LCR_PAR_NONE  (0 << 3) // No Parity.

//c7 = 11000111

// FIFO Control register
#define FCR_ENABLE (1 << 0)
#define FCR_RFRES  (1 << 1)
#define FCR_XFRES  (1 << 2)
#define FCR_DMASEL (1 << 3)
#define FCR_RXTRIG (3 << 6)

// Interrupt Enable register
#define IER_NOINTS (0)

// Modem Control Register
#define MCR_DTR    (1 << 0)
#define MCR_RTS    (1 << 1)
#define MCR_OUT1   (1 << 2)
#define MCR_OUT2   (1 << 3)
#define MCR_LOOPBK (1 << 4) // loop-back

// Line status register
#define LSR_RBF    (1 << 0)
#define LSR_OE     (1 << 1)
#define LSR_PE     (1 << 2)
#define LSR_FE     (1 << 3)
#define LSR_BREAK  (1 << 4)
#define LSR_THRE   (1 << 5)
#define LSR_TEMT   (1 << 6)
#define LSR_FIFOER (1 << 7)

// Modem status register
#define MSR_DCTS   (1 << 0)
#define MSR_DDSR   (1 << 1)
#define MSR_TERI   (1 << 2)
#define MSR_DDCD   (1 << 3)
#define MSR_CTS    (1 << 4)
#define MSR_DSR    (1 << 5)
#define MSR_RI     (1 << 6)
#define MSR_DCD    (1 << 7)

// Interrupt Enable register
#define IER_ERBFI  (1 << 0) //Enable receive  buffer full  interrupt
#define IER_ETBEI  (1 << 1) //Enable transmit buffer empty interrupt
#define IER_ELSI   (1 << 2) //Enable line status interrupt
#define IER_EDSSI  (1 << 3) //Enable delta status signals interrupt

// Interrupt ID Register
#define IID_PEND   (1 << 0) // An interrupt is pending if this is zero
#define IID_0      (1 << 1)
#define IID_1      (1 << 2)
#define IID_2      (1 << 3)

// Port offsets
#define S_RBR 0x00 // Receive buffer register (read only) same as...
#define S_THR 0x00 // Transmitter holding register (write only)
#define S_IER 0x01 // Interrupt enable register
#define S_IIR 0x02 // Interrupt ident register (read only)...
#define S_FCR 0x02 // FIFO control register (write only)
#define S_LCR 0x03 // Line control register
#define S_MCR 0x04 // Modem control register
#define S_LSR 0x05 // Line status register
#define S_MSR 0x06 // Modem status register

#define S_CHECK_BYTE 0xCA

#endif//BORON_HAL_IAPC_UART_H
