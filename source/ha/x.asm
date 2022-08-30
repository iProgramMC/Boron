; Boron - CPU abstraction functions
bits 64

; these functions set the CR3
global HalSetCurrentPageTable
global HalGetCurrentPageTable

HalGetCurrentPageTable:
	mov rax, cr3
	ret

HalSetCurrentPageTable:
	mov cr3, rdi
	ret
