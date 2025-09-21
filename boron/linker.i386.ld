/* Tell the linker that we want an ELF32 output file */
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

/* Linker script for the OS */

OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

ENTRY (KeLowerEntry)

/* Here is where all of the sections of the kernel are defined. */
SECTIONS
{
	/* Begin loading at 0x100000, as that's where GRUB will place our data. */
	. = 1M;
	
	_kernel_start = .;
	
	/* start blocking out writes from here */
	l_code_and_rodata_start = 0xC0100000;

	.lowdata :
	{
		*(.lowdata .lowdata.*)
	}

	.lowtext :
	{
		*(.lowtext .lowtext.*)
	}
	
	. += 0xC0000000;
	
	.text ALIGN (4K) : AT (ADDR (.text) - 0xC0000000)
	{
		*(.text .text.*)
	}
	
	/* TODO: Get these working. */
	KiTextInitStart = .;
	KiTextInitEnd = .;
	KiTextPageStart = .;
	KiTextPageEnd = .;
	
	.rodata ALIGN (4K) : AT (ADDR (.rodata) - 0xC0000000)
	{
		*(.rodata .rodata.*)
		PROVIDE(KiSymbolTable = .);
		PROVIDE(KiSymbolTableEnd = .);
	}
	
	.data ALIGN (4K) : AT (ADDR (.data) - 0xC0000000)
	{
		*(.data .data.*)
	}
	
	.bss ALIGN (4K) : AT (ADDR (.bss) - 0xC0000000)
	{
		*(COMMON)
		*(.bss .bss.*)
		
		/* Hack to keep the PsSystemProcess symbol while adding an object header on top */
		PROVIDE(PsSystemProcess = PspSystemProcessObject + 64);
	}
	
	.section : 
	{
	
	}
	
	/* Discard .note.* and .eh_frame since they may cause issues on some hosts. */
	/DISCARD/ : {
		*(.eh_frame)
		*(.note .note.*)
	}
}