/* Tell the linker that we want an x86_64 ELF64 output file */
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

/* We want the symbol KiSystemStartup to be our entry point */
ENTRY(KiSystemStartup)

/* Define the program headers we want so the bootloader gives us the right */
/* MMU permissions */
PHDRS
{
	text	PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
	rodata  PT_LOAD    FLAGS((1 << 2)) ;            /* Read only */
	data	PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
}

SECTIONS
{
	/* We want to be placed in the topmost 2GiB of the address space, for optimizations, and because that is what the Limine spec mandates. */
	/* Any address in this region will do, but often 0xffffffff80000000 is chosen as that is the beginning of the region. */
	. = 0xffffffff80000000;

	/* Define the regular text section. */
	.text : {
		*(.text)
	} :text
	. = ALIGN(CONSTANT(MAXPAGESIZE));
	
	/* Define the initialization text section, which will be permanently reclaimed after system initialization. */
	KiTextInitStart = .;
	.text.init : {
		*(.text.init)
	} :text
	. = ALIGN(CONSTANT(MAXPAGESIZE));
	KiTextInitEnd   = .;
	
	/* Define the paged text section, which may not be resident at all times, pages may be swapped out and
	   back in at the memory manager's discretion.  */
	KiTextPageStart = .;
	.text.page : {
		*(.text.page)
	} :text
	. = ALIGN(CONSTANT(MAXPAGESIZE));
	KiTextPageEnd   = .;
	
	/* Glob all of the other text sections into .text. */
	.text : {
		*(.text.*)
	} : text

	/* Move to the next memory page for .rodata */
	. = ALIGN(CONSTANT(MAXPAGESIZE));

	.rodata : {
		*(.rodata .rodata.*)
		PROVIDE(KiSymbolTable = .);
		PROVIDE(KiSymbolTableEnd = .);
	} :rodata

	/* Move to the next memory page for .data */
	. = ALIGN(CONSTANT(MAXPAGESIZE));

	/* Global constructor array. */
	.init_array : {
		g_init_array_start = .;
		*(.init_array)
		g_init_array_end = .;
	}
	
	/* Global destructor array. */
	.fini_array : {
		g_fini_array_start = .;
		*(.fini_array)
		g_fini_array_end = .;
	}

	.data : {
		*(.data .data.*)
	} :data
	
	.bss : {
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
