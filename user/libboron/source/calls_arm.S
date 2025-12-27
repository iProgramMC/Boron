// The Boron Operating System
// Copyright (C) 2025 iProgramInCpp

#ifdef TARGET_ARM

.macro CALL number, argcount, name
	.global \name
	.type   \name, %function
\name:
	@ TODO
	mov r0, #1
	bx  lr
.endm

#include "calltbl.h"

#endif
