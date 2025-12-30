ENTRY(KiBeforeSystemStartup)

PHDRS {
	ipldata PT_LOAD FLAGS(4 | 2);  /* R | W */
	ipltext PT_LOAD FLAGS(4 | 1);  /* R | W */
	text    PT_LOAD FLAGS(4 | 1); /* R | X */
	data    PT_LOAD FLAGS(4 | 2); /* R | W */
}

SECTIONS {
	/* change this for other platforms?! or just make the bootloader do this instead
	   of the kernel? TODO */
    . = 0x00000000;
	KiKernelStart = 0xC0000000 + .;
	
	.iplbss ALIGN (16K) : AT (ADDR(.iplbss)) {
		KEEP(*(.iplbss*))
	} :ipldata
	
	.ipltext ALIGN (4K) : AT (ADDR(.ipltext)) {
		__iplbss_start = .;
		KEEP(*(.ipltext*))
		__iplbss_end = .;
	} :ipltext
	
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
    } :text

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
    } :text

    .data ALIGN (4K) : AT (ADDR (.data) - 0xC0000000) {
        *(.data*)
    } :data

    .bss ALIGN (4K) : AT (ADDR (.bss) - 0xC0000000) {
        *(COMMON)
        *(.bss*)
		
		/* Hack to keep the PsSystemProcess symbol while adding an object header on top */
		PROVIDE(PsSystemProcess = PspSystemProcessObject + 64);
    } :data
	
	KiKernelEnd = .;
}
