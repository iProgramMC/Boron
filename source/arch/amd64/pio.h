// Boron - Hardware abstraction.
// x86 specific port I/O
#ifndef NS64_PIO_H
#define NS64_PIO_H

uint8_t KePortReadByte(uint16_t portNo);
void KePortWriteByte(uint16_t portNo, uint8_t data);

#endif//NS64_PIO_H
