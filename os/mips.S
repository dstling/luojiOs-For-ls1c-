#include "ls1c_regdef.h"
#include "ls1c_mipsregs.h"
#include "ls1c_asm.h"


#include "mips_def.h"
#include "stackframe.h"
#include "stackframe_fpu.h"
#define	NOP8	nop;nop;nop;nop;nop;nop;nop;nop

.set	noreorder
.data
.global LOAD_THREAD_STACK	//load elf调用的stack地址
.common LOAD_THREAD_STACK, 128*8*4	//1024

.data
.global LOAD_THREAD 		//load elf使用的thread地址
.common LOAD_THREAD, 128*8


	.text
	.globl	MipsException
	.globl	MipsExceptionEnd
MipsException:
	.set	noat
	la	k0, exception_handler
	jr	k0
	nop
	.set	at
MipsExceptionEnd:

//================================================================

	.set	reorder

	.globl	cp0_get_cause
cp0_get_cause:
	mfc0	v0, CP0_CAUSE
	jr	ra
	nop

	.globl	cp0_get_status
cp0_get_status:
	mfc0	v0, CP0_STATUS
	jr	ra
	nop

	.globl	cp0_get_hi
cp0_get_hi:
	mfhi	v0
	jr	ra
	nop

	.globl	cp0_get_lo
cp0_get_lo:
	mflo	v0
	jr	ra
	nop

//===========================================================
	.set noreorder
	.extern tlb_refill_handler
	/* Exception Handler */
	/* 0x0 - TLB refill handler */
	//.section .vectors.1, "ax", %progbits
	.global tlb_refill_exception
	//.type tlb_refill_exception,@function
tlb_refill_exception:
	j	tlb_refill_handler
	nop

	.set noreorder
	.globl general_exception
general_exception:
	mfc0	k1, CP0_CAUSE
	andi	k1, k1, 0x7c
	lw	k0, exception_handlers(k1)
	jr	k0
	nop

//=======================================================================

	.set reorder
//*/
	.set noreorder
	.globl handle_int2
handle_int2:
	SAVE_ALL
	//jal	  plat_irq_dispatch//dstling remove
	//nop
	jal 	rt_interrupt_enter//dstling add
	nop
	jal 	plat_irq_dispatch
	nop
	jal 	rt_interrupt_leave
	nop
	RESTORE_ALL_AND_RET

	.set reorder
	.set noreorder
	.globl handle_reserved
handle_reserved:
	.set	mips3
	eret
	.set	mips0
	.set reorder

//=================================================================




#include <cpu.h>
#include "context_gcc.h"

LEAF(_go)
	li	v0, SR_EXL		/* Precaution */
	mtc0	v0, COP_0_STATUS_REG
	
	la	k0, LOAD_THREAD_STACK
/*
	LOAD	t0, WATCHLO * RSIZE(k0)
	LOAD	t1, WATCHHI * RSIZE(k0)
	MTC0	t0, COP_0_WATCH_LO
	mfc0	t0, COP_0_PRID
	MTC0	t1, COP_0_WATCH_HI
	srl 	t1, t0, 8
	bne 	t1, MIPS_RM7000, 1f //bne当两个寄存器中内容不等时转移发生
	LOAD	t0, WATCH1 * RSIZE(k0)
	
	// RM7000 specific 
	LOAD	t1, WATCH2 * RSIZE(k0)
	mtc0	t0, COP_0_WATCH_1
	LOAD	t0, WATCHM * RSIZE(k0)
	mtc0	t1, COP_0_WATCH_2
	LOAD	t1, PCOUNT * RSIZE(k0)
	mtc0	t0, COP_0_WATCH_M
	LOAD	t0, PCTRL * RSIZE(k0)
	mtc0	t1, COP_0_PC_COUNT
	LOAD	t1, ICR * RSIZE(k0)
	mtc0	t0, COP_0_PC_CTRL
	mtc0	t1, COP_0_ICR

1:
*/
	LOAD	v0, PC * RSIZE(k0)
	LOAD	v1, CAUSE * RSIZE(k0)
	MTC0	v0, COP_0_EXC_PC
	LOAD	v0, SR * RSIZE(k0)
	mtc0	v1, COP_0_CAUSE_REG
	or	v0, SR_EXL		/* Keep Exeption level status */
	mtc0	v0, COP_0_STATUS_REG

	LOAD	v0, MULHI * RSIZE(k0)
	LOAD	v1, MULLO * RSIZE(k0)
	mthi	v0
	mtlo	v1

	.set	noat

	LOAD	AT, AST * RSIZE(k0)
	LOAD	v0, V0 * RSIZE(k0)
	LOAD	v1, V1 * RSIZE(k0)
	LOAD	a0, A0 * RSIZE(k0)
	LOAD	a1, A1 * RSIZE(k0)
	LOAD	a2, A2 * RSIZE(k0)
	LOAD	a3, A3 * RSIZE(k0)
	LOAD	t0, T0 * RSIZE(k0)
	LOAD	t1, T1 * RSIZE(k0)
	LOAD	t2, T2 * RSIZE(k0)
	LOAD	t3, T3 * RSIZE(k0)
	LOAD	t4, T4 * RSIZE(k0)
	LOAD	t5, T5 * RSIZE(k0)
	LOAD	t6, T6 * RSIZE(k0)
	LOAD	t7, T7 * RSIZE(k0)
	LOAD	s0, S0 * RSIZE(k0)
	LOAD	s1, S1 * RSIZE(k0)
	LOAD	s2, S2 * RSIZE(k0)
	LOAD	s3, S3 * RSIZE(k0)
	LOAD	s4, S4 * RSIZE(k0)
	LOAD	s5, S5 * RSIZE(k0)
	LOAD	s6, S6 * RSIZE(k0)
	LOAD	s7, S7 * RSIZE(k0)
	LOAD	t8, T8 * RSIZE(k0)
	LOAD	t9, T9 * RSIZE(k0)
	LOAD	k1, K1 * RSIZE(k0)
	LOAD	gp, GP * RSIZE(k0)
	LOAD	sp, SP * RSIZE(k0)
	LOAD	s8, S8 * RSIZE(k0)
	LOAD	ra, RA * RSIZE(k0)
	LOAD	k0, K0 * RSIZE(k0)

	.set at
	.set push
	.set mips3
	eret
	.set pop
END(_go)

LEAF(exception_handler)
	.set	noat
	la	k0, start - 1024

	STORE	AT, AST * RSIZE(k0)
	STORE	v0, V0 * RSIZE(k0)
	STORE	v1, V1 * RSIZE(k0)
	STORE	a0, A0 * RSIZE(k0)
	STORE	a1, A1 * RSIZE(k0)
	STORE	a2, A2 * RSIZE(k0)
	STORE	a3, A3 * RSIZE(k0)
	STORE	t0, T0 * RSIZE(k0)
	STORE	t1, T1 * RSIZE(k0)
	STORE	t2, T2 * RSIZE(k0)
	STORE	t3, T3 * RSIZE(k0)
	STORE	t4, T4 * RSIZE(k0)
	STORE	t5, T5 * RSIZE(k0)
	STORE	t6, T6 * RSIZE(k0)
	STORE	t7, T7 * RSIZE(k0)
	STORE	s0, S0 * RSIZE(k0)
	STORE	s1, S1 * RSIZE(k0)
	STORE	s2, S2 * RSIZE(k0)
	STORE	s3, S3 * RSIZE(k0)
	STORE	s4, S4 * RSIZE(k0)
	STORE	s5, S5 * RSIZE(k0)
	STORE	s6, S6 * RSIZE(k0)
	STORE	s7, S7 * RSIZE(k0)
	STORE	t8, T8 * RSIZE(k0)
	STORE	t9, T9 * RSIZE(k0)
	STORE	k1, K1 * RSIZE(k0)
	STORE	gp, GP * RSIZE(k0)
	STORE	sp, SP * RSIZE(k0)
	STORE	s8, S8 * RSIZE(k0)
	STORE	ra, RA * RSIZE(k0)

	.set	at

	mfhi	v0
	mflo	v1
	STORE	v0, MULHI * RSIZE(k0)
	STORE	v1, MULLO * RSIZE(k0)

	mfc0	a0, COP_0_STATUS_REG
	mfc0	v1, COP_0_CAUSE_REG
	STORE	a0, SR * RSIZE(k0)
	MFC0	v0, COP_0_BAD_VADDR
	STORE	v1, CAUSE * RSIZE(k0)
	MFC0	v1, COP_0_EXC_PC
	STORE	v0, BADVADDR * RSIZE(k0)
	STORE	v1, PC * RSIZE(k0)

	MFC0	v0, COP_0_TLB_CONTEXT
	MFC0	v1, COP_0_TLB_XCONTEXT
	STORE	v0, CONTX * RSIZE(k0)
	STORE	v1, XCONTX * RSIZE(k0)
	MFC0	v0, COP_0_TLB_HI
	MFC0	v1, COP_0_TLB_LO0
	STORE	v0, ENTHI * RSIZE(k0)
	MFC0	v0, COP_0_TLB_LO1
	STORE	v1, ENTLO0 * RSIZE(k0)
	STORE	v0, ENTLO1 * RSIZE(k0)

	MFC0	t0, COP_0_WATCH_LO
	MFC0	t1, COP_0_WATCH_HI
	STORE	t0, WATCHLO * RSIZE(k0)
	MFC0	t0, COP_0_PRID
	STORE	t1, WATCHHI * RSIZE(k0)
	MTC0	zero, COP_0_WATCH_LO
	MTC0	zero, COP_0_WATCH_HI
	srl	t1, t0, 8
	bne	t1, MIPS_RM7000, 1f
	STORE	t0, PRID * RSIZE(k0)

	mfc0	t0, COP_0_WATCH_1
	mfc0	t1, COP_0_WATCH_2
	STORE	t0, WATCH1 * RSIZE(k0)
	mfc0	t0, COP_0_WATCH_M
	STORE	t1, WATCH2 * RSIZE(k0)
	mfc0	t1, COP_0_PC_COUNT
	STORE	t0, WATCHM * RSIZE(k0)
	mfc0	t0, COP_0_PC_CTRL
	STORE	t1, PCOUNT * RSIZE(k0)
	mfc0	t1, COP_0_ICR
	STORE	t0, PCTRL * RSIZE(k0)
	STORE	t1, ICR * RSIZE(k0)

1:
	addu	sp, k0, -64		/* Get a new stack */

	and     t0, a0, ~(SR_EXL | SR_KSU_MASK | SR_INT_ENAB)
	mtc0	t0, COP_0_STATUS_REG
	NOP8
	la	gp, _gp

	jal	exception
	move	a0, k0
	/*NORETURN*/
END(exception_handler)


LEAF(setjmp)
	STORE	s0, RSIZE * 0(a0)
	STORE	s1, RSIZE * 1(a0)
	STORE	s2, RSIZE * 2(a0)
	STORE	s3, RSIZE * 3(a0)
	STORE	s4, RSIZE * 4(a0)
	STORE	s5, RSIZE * 5(a0)
	STORE	s6, RSIZE * 6(a0)
	STORE	s7, RSIZE * 7(a0)
	STORE	s8, RSIZE * 8(a0)
	STORE	sp, RSIZE * 9(a0)
	STORE	gp, RSIZE * 10(a0)
	STORE	ra, RSIZE * 11(a0)
	jr	ra
	li	v0, 0			# setjmp return
END(setjmp)

LEAF(longjmp)
	LOAD	ra, RSIZE * 11(a0)
	LOAD	s0, RSIZE * 0(a0)
	LOAD	s1, RSIZE * 1(a0)
	LOAD	s2, RSIZE * 2(a0)
	LOAD	s3, RSIZE * 3(a0)
	LOAD	s4, RSIZE * 4(a0)
	LOAD	s5, RSIZE * 5(a0)
	LOAD	s6, RSIZE * 6(a0)
	LOAD	s7, RSIZE * 7(a0)
	LOAD	s8, RSIZE * 8(a0)
	LOAD	sp, RSIZE * 9(a0)
	LOAD	gp, RSIZE * 10(a0)
	jr	ra
	move v0,a1 #li	v0, 1			# longjmp return
END(longjmp)



///*
LEAF(CPU_GetCOUNT)
	mfc0	v0, COP_0_COUNT
	nop
	j	ra
	nop
END(CPU_GetCOUNT)

LEAF(CPU_SetCOUNT)
	mtc0	a0, COP_0_COUNT
	nop
	j	ra
	nop
END(CPU_GetCOUNT)


LEAF(CPU_SetSR)//CPU_SetSR(a0,a1)  (0, SR_BOOT_EXC_VEC);
    mfc0    v0, COP_0_STATUS_REG
    not 	v1, a1
    and 	v1, v0
    or  	v1, a0
    mtc0    v1, COP_0_STATUS_REG
    NOP8
    j   ra
    nop
END(CPU_SetSR)

LEAF(md_cputype)
	mfc0	v0, COP_0_PRID
	j		ra
	nop
END(md_cputype)

//*/


