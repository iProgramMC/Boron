
#include <ke.h>

int KeGetVersionNumber()
{
	return VER(__BORON_MAJOR, __BORON_MINOR, __BORON_BUILD);
}
