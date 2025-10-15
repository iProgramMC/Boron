;
;    The Boron Operating System
;    Copyright (C) 2025 iProgramInCpp
; 
; Module name:
;    ke/i386/foreinit.asm
; 	
; Abstract:
;    This module implements the entry point of the Boron kernel
;    for the i386 architecture.
; 	
; Author:
;    iProgramInCpp - 14 October 2025
;
bits 32

; **** Initial Program Loader ****
; This was borrowed from NanoShell (https://github.com/iProgramMC/NanoShellOS)
; but it works well enough for it so it'll probably work here too.

%define BASE_ADDRESS 0xC0000000
%define V2P(k) ((k) - BASE_ADDRESS)

section .ipldata

	; Multiboot v0.6.96 specification
	align 4
	
	; Header
	dd 0x1BADB002         ; Signature
	dd 7                  ; Flags: MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE
	dd - (0x1BADB002 + 7) ; Check Sum
	
	; A.out Kludge - blank because we're an ELF
	dd 0
	dd 0
	dd 0
	dd 0
	dd 0
	
	; Video Mode
	dd 0     ; Require a linear frame buffer
	dd 1024  ; Width
	dd 768   ; Height
	dd 32    ; Bits per pixel

section .ipltext
global KiBeforeSystemStartup
KiBeforeSystemStartup:
	cli
	
	; no need for a stack at this stage
	xor ebp, ebp
	
	; store the provided multiboot data
	mov [V2P(KiMultibootSignature)], eax
	mov [V2P(KiMultibootPointer)],   ebx
	
	; first address to map is 0x00000000. map 2048 pages
	xor esi, esi
	mov ecx, 2048
	
	; get the physical address of the bootstrap page directory
	mov edi, V2P(KiBootstrapPageTables)
	
.LoopFill:
	mov edx, esi
	or  edx, 0x03  ; set present and r/w bits
	mov [edi], edx
	
	add esi, 0x1000
	add edi, 4
	loop .LoopFill
	
	; map the two page tables to both 0x00000000 and 0xC0000000, as well
	; as the page directory itself into the 1023rd entry
	mov dword [V2P(KiBootstrapPageDirectory) +   0 * 4], V2P(KiBootstrapPageTables +    0) + 0x03
	mov dword [V2P(KiBootstrapPageDirectory) +   1 * 4], V2P(KiBootstrapPageTables + 4096) + 0x03
	mov dword [V2P(KiBootstrapPageDirectory) + 768 * 4], V2P(KiBootstrapPageTables +    0) + 0x03
	mov dword [V2P(KiBootstrapPageDirectory) + 769 * 4], V2P(KiBootstrapPageTables + 4096) + 0x03
	mov dword [V2P(KiBootstrapPageDirectory) +1023 * 4], V2P(KiBootstrapPageDirectory)     + 0x03
	
	; set CR3 to the physical address of the page directory
	mov ecx, V2P(KiBootstrapPageDirectory)
	mov cr3, ecx
	
	; set PG and WP bit
	; note: WP bit is ignored/reserved on 80386. should probably test for that?
	mov ecx, cr0
	or  ecx, 0x80010000
	mov cr0, ecx
	
	; jump to higher half
	mov ecx, (KiBeforeSystemStartupHigherHalf)
	jmp ecx

extern KiSystemStartup

section .text
global KiBeforeSystemStartupHigherHalf
KiBeforeSystemStartupHigherHalf:
	; unmap the identity mapping, we don't need it anymore
	mov dword [KiBootstrapPageDirectory + 0], 0
	mov dword [KiBootstrapPageDirectory + 4], 0
	
	; reload CR3 to force a TLB flush (we updated the page directory but TLB
	; isn't aware of that).  NOTE: you can probably just use invlpg.
	mov ecx, cr3
	mov cr3, ecx
	
	; setup the initial stack
	mov esp, KiInitialStack
	
	; GDT will be set up later
	
	call KiSystemStartup
	
	cli
.Stop:
	hlt
	jmp .Stop

section .bss

global KiMultibootSignature
global KiMultibootPointer
global KiBootstrapPageDirectory
global KiBootstrapPageTables

KiMultibootSignature:		resd 1
KiMultibootPointer:			resd 1

alignb 4096
KiInitialStack:				resb 4096
KiBootstrapPageDirectory:	resb 4096
KiBootstrapPageTables:		resb 64 * 4096
