// Boron - Hardware abstraction.
// x86 specific port I/O
#pragma once

uint8_t HalPortReadByte(uint16_t portNo);
void HalPortWriteByte(uint16_t portNo, uint8_t data);
