
#include "riscv.h"

	.macro labs reg, symbol
		lui \reg, %hi(\symbol)
		addi \reg, \reg, %lo(\symbol)
	.endm

	.section ".text.entry"
	.globl _start
_start:
	.option push
	.option norelax
	# reloc self
#ifdef RUN_SRAM
	la		a0, _text_start
	labs	a2, _text_start
	la		a1, _data_end
#else
	la		a0, _data_start_lma
	labs	a2, _data_start
#endif
	la		a1, _data_end_lma
1:
	lw		a3, (a0)
	sw		a3, (a2)
	addi	a0, a0, 4
	addi	a2, a2, 4
	bltu	a0, a1, 1b

	# clear bss
	labs	a0, _bss_start
	labs	a1, _bss_end
2:
	sw		zero, (a0)
	addi	a0, a0, 4
	bltu	a0, a1, 2b

	# enable FPU and accelerator if present
	li		t0, MSTATUS_FS | MSTATUS_XS
	csrs	mstatus, t0

	# initialize global pointer
	labs	gp, __global_pointer$
	labs	sp, _stack_end

	labs	ra, main
	jr		ra

.option pop
