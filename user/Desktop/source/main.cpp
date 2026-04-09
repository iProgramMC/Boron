//
//  main.cpp
//
//  Copyright (C) 2026 iProgramInCpp.
//  Created on 9/4/2026.
//
#include <boron.h>
#include <stdarg.h>
#include <frg/vector.hpp>

frg::vector vc;

class RAIITest
{
public:
	RAIITest() {
		OSPrintf("RAIITest constructed.\n");
	}
	~RAIITest() {
		OSPrintf("RAIITest destructed.\n");
	}
};

extern "C"
int _start(UNUSED int ArgumentCount, UNUSED char** ArgumentArray)
{
	RAIITest t;
	vc.push(1);
	vc.push(2);
	vc.push(3);
	
	for (size_t i = 0; i < vc.size(); i++) {
		OSPrintf("vc[i] = %d\n", vc[i]);
	}
	
	return 0;
}