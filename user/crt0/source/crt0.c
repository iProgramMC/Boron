// NOTE: This must *all* be in 1 file!
void* __dso_handle;

extern int main(int ArgumentCount, char** ArgumentValues);

int _start(int ArgumentCount, char** ArgumentValues)
{
	//CallConstructors() ??
	int ReturnValue = main(ArgumentCount, ArgumentValues);
	//CallDestructors();
	return ReturnValue;
}
