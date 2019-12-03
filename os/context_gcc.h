#define STAND_ARG_SIZE		16
#define STAND_FRAME_SIZE	24
#define STAND_RA_OFFSET		20  
	
	/*
	 * Location of the saved registers relative to ZERO.
	 * Usage is p->p_regs[XX].
	 */
#define ZERO	0
#define AST	1
#define V0	2
#define V1	3
#define A0	4
#define A1	5
#define A2	6
#define A3	7
#define T0	8
#define T1	9
#define T2	10
#define T3	11
#define T4	12
#define T5	13
#define T6	14
#define T7	15
#define S0	16
#define S1	17
#define S2	18
#define S3	19
#define S4	20
#define S5	21
#define S6	22
#define S7	23
#define T8	24
#define T9	25
#define K0	26
#define K1	27
#define GP	28
#define SP	29
#define S8	30
#define RA	31
#define	SR	32
#define	PS	SR	/* alias for SR */
#define MULLO	33
#define MULHI	34
#define BADVADDR 35
#define CAUSE	36
#define	PC	37
	
#define	NUMSAVEREGS 38			/* 普通寄存的数量Number of registers saved in trap */
#define NUMFPREGS 0	//33		//fpu寄存器需要保存的数量 thread.h中frame中已经屏蔽
#define NUMAUXREG 0	//29		//thread.h中frame中已经屏蔽
	
#define FPBASE	(NUMSAVEREGS)//38
#define F0	(FPBASE+0)
#define F1	(FPBASE+1)
#define F2	(FPBASE+2)
#define F3	(FPBASE+3)
#define F4	(FPBASE+4)
#define F5	(FPBASE+5)
#define F6	(FPBASE+6)
#define F7	(FPBASE+7)
#define F8	(FPBASE+8)
#define F9	(FPBASE+9)
#define F10	(FPBASE+10)
#define F11	(FPBASE+11)
#define F12	(FPBASE+12)
#define F13	(FPBASE+13)
#define F14	(FPBASE+14)
#define F15	(FPBASE+15)
#define F16	(FPBASE+16)
#define F17	(FPBASE+17)
#define F18	(FPBASE+18)
#define F19	(FPBASE+19)
#define F20	(FPBASE+20)
#define F21	(FPBASE+21)
#define F22	(FPBASE+22)
#define F23	(FPBASE+23)
#define F24	(FPBASE+24)
#define F25	(FPBASE+25)
#define F26	(FPBASE+26)
#define F27	(FPBASE+27)
#define F28	(FPBASE+28)
#define F29	(FPBASE+29)
#define F30	(FPBASE+30)
#define F31	(FPBASE+31)
#define	FSR	(FPBASE+32)
	
#define AUXBASE (FPBASE + NUMFPREGS)//38+0
#define	COUNT	(AUXBASE+0)
#define	COMPARE	(AUXBASE+1)
#define	WATCHLO	(AUXBASE+2)
#define	WATCHHI	(AUXBASE+3)
#define	WATCHM	(AUXBASE+4)
#define	WATCH1	(AUXBASE+5)
#define	WATCH2	(AUXBASE+6)
#define	LLADR	(AUXBASE+7)
#define	ECC	(AUXBASE+8)
#define	CACHER	(AUXBASE+9)
#define	TAGLO	(AUXBASE+10)
#define	TAGHI	(AUXBASE+11)
#define	WIRED	(AUXBASE+12)
#define	PGMSK	(AUXBASE+13)
#define	ENTLO0	(AUXBASE+14)
#define	ENTLO1	(AUXBASE+15)
#define	ENTHI	(AUXBASE+16)
#define CONTX	(AUXBASE+17)
#define	XCONTX	(AUXBASE+18)
#define INDEX	(AUXBASE+19)
#define	RANDOM	(AUXBASE+20)
#define	CONFIG	(AUXBASE+21)
#define	ICR	(AUXBASE+22)
#define	IPLLO	(AUXBASE+23)
#define	IPLHI	(AUXBASE+24)
#define	PRID	(AUXBASE+25)
#define	PCOUNT	(AUXBASE+26)
#define	PCTRL	(AUXBASE+27)
#define	ERRPC	(AUXBASE+28)

#define	STORE	sw	/* 32 bit mode regsave instruction */
#define	LOAD	lw	/* 32 bit mode regload instruction */
#define	RSIZE	4	/* 32 bit mode register size */

/* RM7000 specific */
#define	MIPS_RM7000	0x27	/* QED RM7000 CPU		ISA IV  */
#define COP_0_WATCH_1		$18
#define COP_0_WATCH_2		$19
#define COP_0_WATCH_M		$24
#define COP_0_PC_COUNT		$25
#define COP_0_PC_CTRL		$22

#define	COP_0_ICR		$20		/* CFC */

#define s8		$30		/* same like fp! */








