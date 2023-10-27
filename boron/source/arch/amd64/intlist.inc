; The Boron Operating System
; Copyright (C) 2023 iProgramInCpp

; Author - iProgramInCpp - 27 October 2023

INT 00, N
INT 01, N
INT 02, N
INT 03, N
INT 04, N
INT 05, N
INT 06, N
INT 07, N
INT 08, Y
INT 09, N
INT 0A, Y
INT 0B, Y
INT 0C, Y
INT 0D, Y
INT 0E, Y
INT 0F, N
INT 10, N
INT 11, Y
INT 12, N
INT 13, N
INT 14, N
INT 15, N
INT 16, N
INT 17, Y
INT 18, N
INT 19, N
INT 1A, N
INT 1B, Y
INT 1C, Y
INT 1D, N
INT 1E, N
INT 1F, N

%macro INTGRP 1
INT %{1}0, N
INT %{1}1, N
INT %{1}2, N
INT %{1}3, N
INT %{1}4, N
INT %{1}5, N
INT %{1}6, N
INT %{1}7, N
INT %{1}8, N
INT %{1}9, N
INT %{1}A, N
INT %{1}B, N
INT %{1}C, N
INT %{1}D, N
INT %{1}E, N
INT %{1}F, N
%endmacro

INTGRP 2
INTGRP 3
INTGRP 4
INTGRP 5
INTGRP 6
INTGRP 7
INTGRP 8
INTGRP 9
INTGRP A
INTGRP B
INTGRP C
INTGRP D
INTGRP E
INTGRP F

%unmacro INTGRP 1
