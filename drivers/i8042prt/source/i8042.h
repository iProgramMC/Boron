/***
	The Boron Operating System
	Copyright (C) 2023-2024 iProgramInCpp

Module name:
	i8042.h

Abstract:
	This module defines constants used by the
	i8042prt PS/2 controller driver.
	
Author:
	iProgramInCpp - 2 April 2024
***/
#pragma once

#include <ke.h>

/* Available ports */
#define I8042_PORT_DATA   (0x60)
#define I8042_PORT_STATUS (0x64)
#define I8042_PORT_CMD    (0x64)

/* Status register flags */
#define I8042_STATUS_OUTPUT_FULL (1 << 0)
#define I8042_STATUS_INPUT_EMPTY (1 << 1)
#define I8042_STATUS_SYSTEM      (1 << 2)
#define I8042_STATUS_MODE_DATA   (1 << 3) // 0 - PS/2 device, 1 - PS/2 controller
#define I8042_STATUS_TIMEOUT_ERR (1 << 6)
#define I8042_STATUS_PARITY_ERR  (1 << 7)

#define I8042_CMD_READ_CONFIG    (0x20)
#define I8042_CMD_WRITE_CONFIG   (0x60)
#define I8042_CMD_DISABLE_PORT_1 (0xAD)
#define I8042_CMD_DISABLE_PORT_2 (0xA7)
#define I8042_CMD_ENABLE_PORT_1  (0xAE)
#define I8042_CMD_ENABLE_PORT_2  (0xA8)

#define I8042_CONFIG_INT_PORT_1  (1 << 0)
#define I8042_CONFIG_INT_PORT_2  (1 << 1)
#define I8042_CONFIG_DISABLE_1   (1 << 4)
#define I8042_CONFIG_DISABLE_2   (1 << 5)
#define I8042_CONFIG_TRANSLATION (1 << 6)
#define I8042_CONFIG_ZERO_BIT_3  (1 << 3)
#define I8042_CONFIG_ZERO_BIT_7  (1 << 7)

#define I8042_IRQ_KBD (0x1)
#define I8042_IRQ_MOU (0xC)

// kbd.c
void KbdInitialize(int Vector, KIPL Ipl);

// i8042.c
void SendByte(uint16_t Port, uint8_t Value);
uint8_t GetByte(uint16_t Port);
uint8_t ReadConfig();
void WriteConfig(uint8_t Config);

// main.c
int AllocateVector(PKIPL Ipl, KIPL Default);
