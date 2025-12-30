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
		KiTextInitStart = .;
		*(.text.init)
		KiTextInitEnd = .;
		. = ALIGN(CONSTANT(MAXPAGESIZE));
		
		KiTextPageStart = .;
		*(.text.page)
		KiTextPageEnd = .;
		. = ALIGN(CONSTANT(MAXPAGESIZE));
		
        *(.text*)
    }

    .rodata ALIGN (4K) : AT (ADDR (.rodata) - 0xC0000000) {
		KiInitArrayStart = .;
		*(.init_array .init_array.*)
		KiInitArrayEnd = .;
		KiFiniArrayStart = .;
		*(.fini_array .fini_array.*)
		KiFiniArrayEnd = .;
		
        *(.rodata*)
		PROVIDE(KiSymbolTable = .);
		PROVIDE(KiSymbolTableEnd = .);
    }

    .data ALIGN (4K) : AT (ADDR (.data) - 0xC0000000) {
        *(.data*)
    }

    .bss ALIGN (4K) : AT (ADDR (.bss) - 0xC0000000) {
        *(COMMON)
        *(.bss*)
		
		/* Hack to keep the PsSystemProcess symbol while adding an object header on top */
		PROVIDE(PsSystemProcess = PspSystemProcessObject + 64);
    }
	
	KiKernelEnd = .;
}
