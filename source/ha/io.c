#include <main.h>
#include "io.h"

uint8_t HalPortReadByte(uint16_t portNo)
{
    uint8_t rv;
    ASM("inb %1, %0" : "=a" (rv) : "dN" (portNo));
    return rv;
}

void HalPortWriteByte(uint16_t portNo, uint8_t data)
{
	ASM("outb %0, %1"::"a"((uint8_t)data),"Nd"((uint16_t)portNo));
}
