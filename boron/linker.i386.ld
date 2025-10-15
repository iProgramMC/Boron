/* Tell the linker that we want an ELF32 output file */
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

/* Linker script for the OS */

OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

ENTRY (KiBeforeSystemStartup)

/* Define the program headers we want so the bootloader gives us the right */
/* MMU permissions */
PHDRS
{
	intext	PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
	indata	PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
	text	PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
	rodata  PT_LOAD    FLAGS((1 << 2)) ;            /* Read only */
	data	PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
}

/* Here is where all of the sections of the kernel are defined. */
SECTIONS
{
	/* Begin loading at 0x100000, as that's where GRUB will place our data. */
	. = 1M;
	
	/* start blocking out writes from here */
	KiReadOnlyStart = . + 0xC0000000;

	.ipldata :
	{
		*(.ipldata .ipldata.*)
	} :indata

	.ipltext :
	{
		*(.ipltext .ipltext.*)
	} :intext
	
	. += 0xC0000000;
	
	.text.init ALIGN (4K) : AT (ADDR (.text.init) - 0xC0000000)
	{
	} :text
	
	.text.page ALIGN (4K) : AT (ADDR (.text.page) - 0xC0000000)
	{
	} :text
	
	.text ALIGN (4K) : AT (ADDR (.text) - 0xC0000000)
	{
		KiTextInitStart = .;
		*(.text.init)
		KiTextInitEnd = .;
		. = ALIGN(CONSTANT(MAXPAGESIZE));
		
		KiTextPageStart = .;
		*(.text.page)
		KiTextPageEnd = .;
		. = ALIGN(CONSTANT(MAXPAGESIZE));
		
		*(.text .text.*)
	} :text
	
	.rodata ALIGN (4K) : AT (ADDR (.rodata) - 0xC0000000)
	{
		KiInitArrayStart = .;
		*(.init_array .init_array.*)
		KiInitArrayEnd = .;
		KiFiniArrayStart = .;
		*(.fini_array .fini_array.*)
		KiFiniArrayEnd = .;
		
		*(.rodata .rodata.*)
		PROVIDE(KiSymbolTable = .);
		PROVIDE(KiSymbolTableEnd = .);
	} :rodata
	
	KiReadOnlyEnd = .;
	
	.data ALIGN (4K) : AT (ADDR (.data) - 0xC0000000)
	{
		/* place the end right where data starts to get that nice page alignment :) */
		*(.data .data.*)
	} :data
	
	.bss ALIGN (4K) : AT (ADDR (.bss) - 0xC0000000)
	{
		*(COMMON)
		*(.bss .bss.*)
		
		/* Hack to keep the PsSystemProcess symbol while adding an object header on top */
		PROVIDE(PsSystemProcess = PspSystemProcessObject + 64);
	} :data
	
	/* Discard .note.* and .eh_frame since they may cause issues on some hosts. */
	/DISCARD/ : {
		*(.eh_frame)
		*(.note .note.*)
	}
}