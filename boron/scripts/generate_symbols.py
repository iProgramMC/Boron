#!/usr/bin/python3
# The Boron Operating System - Copyright (C) 2023 iProgramInCpp
#
# This Python script generates the symbol definitions.
import sys

print('/********** The Boron Operating System **********/')
print('#include <rtl/symdefs.h>')
print('const KSYMBOL KiSymbolTable[] = {')

Count = 0

for Line in sys.stdin:
    Line = Line.rstrip()
    Tokens = Line.split()
    Name = Tokens[0]
    Type = Tokens[1]
    Address = Tokens[2]
    
    if Type == 'T' or Type == 't':
        print(f'{{ 0x{Address}ull, "{Name}" }},');
    
    Count += 1

print('};')
print(f'const KSYMBOL KiSymbolTableEnd[] = {{ {{ 0x0, "" }} }};')
