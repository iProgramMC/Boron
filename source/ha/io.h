// Boron - Hardware abstraction.
// x86 specific port I/O
#pragma once

uint8_t KePortReadByte(uint16_t portNo);
void KePortWriteByte(uint16_t portNo, uint8_t data);
