

TEXT_START = DEFINED(TEXT_START) ? TEXT_START : 0x20000000;
STACK_SIZE = DEFINED(STACK_SIZE) ? STACK_SIZE : 0x00001000;


ENTRY(_start)

MEMORY
{
	FLASH  (rx) : ORIGIN = 0x80000000, LENGTH = 1M
	SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{
	.text TEXT_START :
	{
		_text_start = .;
		*(.text.entry)
		*(.text)
		*(.text.*)
		_text_end = .;
	} >FLASH AT>FLASH

	.rodata : ALIGN(16)
	{
		_rodata_start = .;
		*(.rodata)
		*(.rodata.*)
		*(.rodata1)
		*(.sdata2)
		*(.sdata2.*)
		*(.sbss2)
		*(.sbss2.*)
		. = ALIGN(4);
		__usbh_class_info_start__ = .;
		KEEP(*(.usbh_class_info))
		__usbh_class_info_end__ = .;
	} >FLASH AT>FLASH

	.data 0x20000000 :  ALIGN(16)
	{
		_data_start = .;
		*(.data)
		*(.data.*)
		*(.data1)
		__global_pointer$ = . + 0x7f0;
		*(.sdata)
		*(.sdata.*)
		*(.got.plt)
		*(.got)
		*(.dynamic)
		*(.fixup);
		_data_end = .;
	} >SRAM AT>FLASH
	_data_start_lma = LOADADDR(.data);
	_data_end_lma = LOADADDR(.data) + SIZEOF(.data);

	.bss :  ALIGN(16)
	{
		_bss_start = .;
		*(.sbss)
		*(.sbss.*)
		*(.scommon)
		*(.dynbss)
		*(.bss)
		*(.bss.*)
		. = ALIGN(16);
		_stack_start = .;
		. = . + STACK_SIZE;
		_stack_end = .;
		*(COMMON) ;
		_bss_end = . ;
	} >SRAM
	_end = .;

	. = ALIGN(256);
	_heap_start = .;
	_heap_size = ORIGIN(SRAM) + LENGTH(SRAM) - _heap_start;

	/*
	   These debug sections are here for information only - they're not going to be
	   included in the ROM-RAM copy because it only copies .text, .data, and .bss.
	 */
	.stab 0 : { *(.stab) }
	.stabstr 0 : { *(.stabstr) }

	/* DWARF debug sections */
	.debug 0 : {*(.debug)}
	.debug_srcinfo 0 : {*(.debug_srcinfo)}
	.debug_aranges 0 : {*(.debug_aranges)}
	.debug_pubnames 0 : {*(.debug_pubnames)}
	.debug_sfnames 0 : {*(.debug_sfnames)}
	.line 0 : {*(.line)}
	.gnu : { *(.gnu*) }
	.note : { *(.note*) }

	.dynsym : { *(.dynsym) }
	.dynbss : { *(.dynbss) }
	.dynstr : { *(.dynstr*) }
	.dynamic : { *(.dynamic*) }
	.hash : { *(.hash*) }
	.plt : { *(.plt*) }
	.interp : { *(.interp*) }
	.gnu : { *(.gnu*) }
}
