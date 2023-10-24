#include <main.h>

int* Test()
{
	static int stuff;
	return &stuff;
}

int DriverEntry()
{
	LogMsg("Hisssssssssssssssss, Viper Lives");
	
	*Test() = 69;
	
	return 42 + *Test();
}
