PHDRS {
	limine_requests PT_LOAD;
	text PT_LOAD;
	rodata PT_LOAD;
	data PT_LOAD;
	dynamic PT_DYNAMIC;
}

SECTIONS {
	. = 0xFFFFFFFF80000000;
	HIDDEN(KERNEL_START = .);

	HIDDEN(REQUESTS_START = .);
	.limine_requests : {
		KEEP(*(.limine_requests_start))
		KEEP(*(.limine_requests))
		KEEP(*(.limine_requests_end))
	} :limine_requests
	HIDDEN(REQUESTS_END = .);

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	HIDDEN(TEXT_START = .);
	.text : {
		*(.text .text.*)
	} :text
	HIDDEN(TEXT_END = .);

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	HIDDEN(RODATA_START = .);
	.rodata : {
		*(.rodata .rodata.*)
	} :rodata

	.init_array : {
		HIDDEN(INIT_ARRAY_START = .);
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		KEEP(*(.init_array))
		HIDDEN(INIT_ARRAY_END = .);
	} :rodata
	HIDDEN(RODATA_END = .);

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	HIDDEN(DATA_START = .);
	.data : {
		*(.data .data.*)
	} :data

	.dynamic : {
		*(.dynamic)
	} :data :dynamic

	.bss : {
		*(.bss .bss.*)
		*(COMMON)
	} :data
	HIDDEN(DATA_END = .);

	/DISCARD/ : {
		*(.eh_frame*)
		*(.note .note.*)
		*(.interp)
	}
}
