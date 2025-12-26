ENTRY(KiBeforeSystemStartup)

SECTIONS {
	/* change this for other platforms?! or just make the bootloader do this instead
	   of the kernel? TODO */
    . = 0x00000000;
	KiKernelStart = 0xC0000000 + .;
	
	.ipltext ALIGN (4K) : AT (0x00000000) {
		KEEP(*(.ipltext*))
	}
	
	.iplbss : ALIGN(16K) {
		KEEP(*(.iplbss*))
	}
	
	. += 0xC0000000;

    .text ALIGN (4K) : AT (ADDR (.text) - 0xC0000000) {
        *(.text*)
    }

    .rodata ALIGN (4K) : AT (ADDR (.rodata) - 0xC0000000) {
        *(.rodata*)
    }

    .data ALIGN (4K) : AT (ADDR (.data) - 0xC0000000) {
        *(.data*)
    }

    .bss ALIGN (4K) : AT (ADDR (.bss) - 0xC0000000) {
        *(COMMON)
        *(.bss*)
    }
	
	KiKernelEnd = .;
}
