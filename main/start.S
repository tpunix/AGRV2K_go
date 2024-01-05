
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


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


	.global arch_set_context
	.global _trap_entry
	.extern task_current
	.align 4
_trap_entry:
	csrrw	a0, mscratch, a0
	lw		a0, task_current

	sw		x1,   0*4(a0)
	sw		x2,   1*4(a0)
	sw		x3,   2*4(a0)
	sw		x4,   3*4(a0)
	sw		x5,   4*4(a0)
	sw		x6,   5*4(a0)
	sw		x7,   6*4(a0)
	sw		x8,   7*4(a0)
	sw		x9,   8*4(a0)
//	sw		x10,  9*4(a0)
	sw		x11, 10*4(a0)
	sw		x12, 11*4(a0)
	sw		x13, 12*4(a0)
	sw		x14, 13*4(a0)
	sw		x15, 14*4(a0)
	sw		x16, 15*4(a0)
	sw		x17, 16*4(a0)
	sw		x18, 17*4(a0)
	sw		x19, 18*4(a0)
	sw		x20, 19*4(a0)
	sw		x21, 20*4(a0)
	sw		x22, 21*4(a0)
	sw		x23, 22*4(a0)
	sw		x24, 23*4(a0)
	sw		x25, 24*4(a0)
	sw		x26, 25*4(a0)
	sw		x27, 26*4(a0)
	sw		x28, 27*4(a0)
	sw		x29, 28*4(a0)
	sw		x30, 29*4(a0)
	sw		x31, 30*4(a0)

	csrrw	a1, mscratch, a0
	sw		a1,  9*4(a0)  // a0
	csrr	a1, mepc
	sw		a1,  31*4(a0) // pc
	csrr	a1, mcause
	call	trap_handle

	lw		a0, task_current

arch_set_context:
	lw		a1,  31*4(a0) // pc
	csrw	mepc, a1
	lw		x1,   0*4(a0)
	lw		x2,   1*4(a0)
	lw		x3,   2*4(a0)
	lw		x4,   3*4(a0)
	lw		x5,   4*4(a0)
	lw		x6,   5*4(a0)
	lw		x7,   6*4(a0)
	lw		x8,   7*4(a0)
	lw		x9,   8*4(a0)
//	lw		x10,  9*4(a0)
	lw		x11, 10*4(a0)
	lw		x12, 11*4(a0)
	lw		x13, 12*4(a0)
	lw		x14, 13*4(a0)
	lw		x15, 14*4(a0)
	lw		x16, 15*4(a0)
	lw		x17, 16*4(a0)
	lw		x18, 17*4(a0)
	lw		x19, 18*4(a0)
	lw		x20, 19*4(a0)
	lw		x21, 20*4(a0)
	lw		x22, 21*4(a0)
	lw		x23, 22*4(a0)
	lw		x24, 23*4(a0)
	lw		x25, 24*4(a0)
	lw		x26, 25*4(a0)
	lw		x27, 26*4(a0)
	lw		x28, 27*4(a0)
	lw		x29, 28*4(a0)
	lw		x30, 29*4(a0)
	lw		x31, 30*4(a0)
	lw		x10,  9*4(a0) // a0
	mret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	.global arch_swap_context
	.align 4
arch_swap_context:
	mv		a1, a0
	lw		a0, task_current

	sw		x1,   0*4(a0)
	sw		x2,   1*4(a0)
	sw		x3,   2*4(a0)
	sw		x4,   3*4(a0)
	sw		x5,   4*4(a0)
	sw		x6,   5*4(a0)
	sw		x7,   6*4(a0)
	sw		x8,   7*4(a0)
	sw		x9,   8*4(a0)
	sw		x10,  9*4(a0)
	sw		x11, 10*4(a0)
	sw		x12, 11*4(a0)
	sw		x13, 12*4(a0)
	sw		x14, 13*4(a0)
	sw		x15, 14*4(a0)
	sw		x16, 15*4(a0)
	sw		x17, 16*4(a0)
	sw		x18, 17*4(a0)
	sw		x19, 18*4(a0)
	sw		x20, 19*4(a0)
	sw		x21, 20*4(a0)
	sw		x22, 21*4(a0)
	sw		x23, 22*4(a0)
	sw		x24, 23*4(a0)
	sw		x25, 24*4(a0)
	sw		x26, 25*4(a0)
	sw		x27, 26*4(a0)
	sw		x28, 27*4(a0)
	sw		x29, 28*4(a0)
	sw		x30, 29*4(a0)
	sw		x31, 30*4(a0)
	sw		ra,  31*4(a0) // pc

	sw		a1, task_current, a0
	mv		a0, a1
	j		arch_set_context

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

