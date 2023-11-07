
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


	.global _trap_entry
	.align 4
_trap_entry:
	addi	sp, sp, -32*4
	sw		x1,   0*4(sp);
	sw		x2,   1*4(sp);
	sw		x3,   2*4(sp);
	sw		x4,   3*4(sp);
	sw		x5,   4*4(sp);
	sw		x6,   5*4(sp);
	sw		x7,   6*4(sp);
	sw		x8,   7*4(sp);
	sw		x9,   8*4(sp);
	sw		x10,  9*4(sp);
	sw		x11, 10*4(sp);
	sw		x12, 11*4(sp);
	sw		x13, 12*4(sp);
	sw		x14, 13*4(sp);
	sw		x15, 14*4(sp);
	sw		x16, 15*4(sp);
	sw		x17, 16*4(sp);
	sw		x18, 17*4(sp);
	sw		x19, 18*4(sp);
	sw		x20, 19*4(sp);
	sw		x21, 20*4(sp);
	sw		x22, 21*4(sp);
	sw		x23, 22*4(sp);
	sw		x24, 23*4(sp);
	sw		x25, 24*4(sp);
	sw		x26, 25*4(sp);
	sw		x27, 26*4(sp);
	sw		x28, 27*4(sp);
	sw		x29, 28*4(sp);
	sw		x30, 29*4(sp);
	sw		x31, 30*4(sp);

	csrr	a0, mepc
	sw		a0,  31*4(sp);
	csrr	a1, mcause
	mv		a0, sp
	call	trap_handle

	lw		x1,   0*4(sp);
	lw		x2,   1*4(sp);
	lw		x3,   2*4(sp);
	lw		x4,   3*4(sp);
	lw		x5,   4*4(sp);
	lw		x6,   5*4(sp);
	lw		x7,   6*4(sp);
	lw		x8,   7*4(sp);
	lw		x9,   8*4(sp);
	lw		x10,  9*4(sp);
	lw		x11, 10*4(sp);
	lw		x12, 11*4(sp);
	lw		x13, 12*4(sp);
	lw		x14, 13*4(sp);
	lw		x15, 14*4(sp);
	lw		x16, 15*4(sp);
	lw		x17, 16*4(sp);
	lw		x18, 17*4(sp);
	lw		x19, 18*4(sp);
	lw		x20, 19*4(sp);
	lw		x21, 20*4(sp);
	lw		x22, 21*4(sp);
	lw		x23, 22*4(sp);
	lw		x24, 23*4(sp);
	lw		x25, 24*4(sp);
	lw		x26, 25*4(sp);
	lw		x27, 26*4(sp);
	lw		x28, 27*4(sp);
	lw		x29, 28*4(sp);
	lw		x30, 29*4(sp);
	lw		x31, 30*4(sp);
	addi	sp, sp, 32*4

	mret

