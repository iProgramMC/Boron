//
//  main.cpp
//
//  Copyright (C) 2026 iProgramInCpp.
//  Created on 9/4/2026.
//
#include <boron.h>
#include <stdarg.h>
#include <new>
#include <frg/vector.hpp>
#include <frg/std_compat.hpp>

frg::vector<int, frg::stl_allocator> vc;

class RAIITest
{
public:
	RAIITest(const char* _tag) {
		OSPrintf("RAIITest constructed: %s.\n", _tag);
		tag = _tag;
	}
	~RAIITest() {
		OSPrintf("RAIITest destructed: %s.\n", tag);
	}
	
private:
	const char* tag;
};

RAIITest gbl("Global");

extern "C"
int _start(UNUSED int ArgumentCount, UNUSED char** ArgumentArray)
{
	int* x = new int();
	*x = 5;
	OSPrintf("*x = %d\n", *x);
	delete x;
	
	RAIITest t("Local");
	vc.push(1);
	vc.push(2);
	vc.push(3);
	
	for (size_t i = 0; i < vc.size(); i++) {
		OSPrintf("vc[i] = %d\n", vc[i]);
	}
	
	return 0;
}