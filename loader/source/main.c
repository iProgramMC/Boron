#include <stdint.h>
#include <stdbool.h>

#define ASM(...) __asm__ volatile(__VA_ARGS__)

void PortWriteByte(uint16_t portNo, uint8_t data)
{
	ASM("outb %0, %1"::"a"((uint8_t)data),"Nd"((uint16_t)portNo));
}

void PrintChar(char c)
{
	PortWriteByte(0xE9, c);
}

void PrintMessage(const char* Message)
{
	while (*Message)
	{
		PrintChar(*Message);
		Message++;
	}
}

void LdrStartup()
{
	PrintMessage("Hello from the Loader\n");
	
	ASM("cli");
	while (true)
		ASM("hlt");
}
