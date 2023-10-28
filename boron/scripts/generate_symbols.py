#!/usr/bin/python3
# The Boron Operating System - Copyright (C) 2023 iProgramInCpp
#
# This Python script generates the symbol definitions.
# Note: The input piped into it MUST be the output of `nm` with the `-P` switch.

import sys

def SymKey(s):
    return s[0]  # Return the address member

print('; ********** The Boron Operating System **********/')
print('section .rodata')
print('global KiSymbolTable')
print('global KiSymbolTableEnd')
print('KiSymbolTable:')

Names = "section .rodata\n"

SymbolList = []

for Line in sys.stdin:
    Line = Line.rstrip()
    Tokens = Line.split()
    
    Name = Tokens[0]
    Type = Tokens[1]
    Address = int(Tokens[2], 16)
    
    if len(Tokens) < 4:
        size = 1
    else:
        Size = int(Tokens[3], 16)
    
    if Type == 'T' or Type == 't':
        SymbolList.append((Address, Size, Name))

SymbolList.sort(key=SymKey)

Count = 0
for Symbol in SymbolList:
    Address = Symbol[0]
    Size = Symbol[1]
    Name = Symbol[2]

    if Count < len(SymbolList) - 1:
        # Attempt to correct the size of small asm functions
        # that aren't aligned to sixteen bytes. Their size is reported
        # as bigger than it actually is for some reason
        if Address + Size > SymbolList[Count + 1][0]:
            Size = SymbolList[Count + 1][0] - Address
        
        # If the symbol has at most 15 bytes until the next symbol,
        # expand the size to include the padding.
        # Some no_return functions are missed without this fix.
        Thing = (Address + Size + 0xF) & 0xFFFFFFFFFFFFFFF0
        
        if Thing == SymbolList[Count + 1][0]:
            Size = SymbolList[Count + 1][0] - Address
    
    print(f'dq 0x{Address:x}')
    print(f'dq 0x{Size:x}')
    print(f'dq name_{Count}')
    Names += f'name_{Count}: db "{Name}", 0\n'
    Count += 1

print('KiSymbolTableEnd:')
print(Names)
