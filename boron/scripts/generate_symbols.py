#!/usr/bin/python3
# The Boron Operating System - Copyright (C) 2023 iProgramInCpp
#
# This Python script generates the symbol definitions.
import sys

print('; ********** The Boron Operating System **********/')
print('section .rodata')
print('global KiSymbolTable')
print('global KiSymbolTableEnd')
print('KiSymbolTable:')

Count = 0
Names = "section .rodata\n"

for Line in sys.stdin:
    Line = Line.rstrip()
    Tokens = Line.split()
    Name = Tokens[0]
    Type = Tokens[1]
    Address = Tokens[2]
    
    if Type == 'T' or Type == 't':
        print(f'dq 0x{Address}');
        print(f'dq name_{Count}');
        Names += f'name_{Count}: db "{Name}", 0\n'
    
    Count += 1

print('KiSymbolTableEnd:')
print(Names)
