#include <ke.h>
#include <hal.h>
#include <string.h>

void KeCrash(const char* message, ...)
{
	// format the message
	va_list va;
	va_start(va, message);
	char buffer[1024]; // may be a beefier message than a garden variety LogMsg
	buffer[sizeof buffer - 3] = 0;
	int chars = vsnprintf(buffer, sizeof buffer - 3, message, va);
	strcpy(buffer + chars, "\n");
	va_end(va);
	
	HalCrashSystem(buffer);
}
