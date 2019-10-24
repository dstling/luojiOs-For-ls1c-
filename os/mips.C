#include "ls1c_regdef.h"
#include "ls1c_mipsregs.h"
#include "ls1c_asm.h"
#include "ls1c_stackframe.h"
#include "ls1c_cacheops.h"
#include "ls1c_regs.h"
#include "start.h"
#include "cpu.h"
#include "sdram_cfg.h"

#define WAYBITS_HIGH
#define	IndexInvalidate_I	0x00
#define CACHED_MEMORY_ADDR      0x80000000


		.set	noreorder
	
		.data
	//	Register area outgoing
		.global DBGREG
		.common DBGREG, 128*8

	LEAF(md_cachestat)
			jr	ra
			li	v0, 1		/* Always ON */
	END(md_cachestat)


	LEAF(CPU_FlushICache)
#ifdef WAYBITS_HIGH
		lw	t0, CpuPrimaryInstSetSize	# Set size
#else
		li	t0,1
#endif
		addu	a1, 127 			# Align for unrolling
		and a0, 0xffff80			# Align start address
		addu	a0, CACHED_MEMORY_ADDR		# a0 now new KSEG0 address
		srl a1, a1, 7			# Number of unrolled loops
	1:
		lw	v0, CpuNWayICache		# Cache properties
		addiu	v0, -2				# <0 1way, 0 = two, >0 four
		bgez	v0, 2f
		addu	a1, -1
	
		cache	IndexInvalidate_I, 16(a0)	# direct mapped
		cache	IndexInvalidate_I, 48(a0)
		cache	IndexInvalidate_I, 80(a0)
		b	3f
		cache	IndexInvalidate_I, 112(a0)
	
	2:
		addu	t1, t0, a0			# Nway cache, flush set B.
		cache	IndexInvalidate_I, 0(t1)
		cache	IndexInvalidate_I, 32(t1)
		cache	IndexInvalidate_I, 64(t1)
		cache	IndexInvalidate_I, 96(t1)
		beqz	v0, 3f				# Is two way do set A
		addu	t1, t0				# else step to set C.
	
		cache	IndexInvalidate_I, 0(t1)
		cache	IndexInvalidate_I, 32(t1)
		cache	IndexInvalidate_I, 64(t1)
		cache	IndexInvalidate_I, 96(t1)
	
		addu	t1, t0				# step to set D
		cache	IndexInvalidate_I, 0(t1)
		cache	IndexInvalidate_I, 32(t1)
		cache	IndexInvalidate_I, 64(t1)
		cache	IndexInvalidate_I, 96(t1)
	
		addiu	v0, -2				# 0 = 4-way, >0 = 8-way
		beqz	v0, 3f				# If just 4-way, to set A
		addu	t1, t0				# else step to set E.
	
		cache	IndexInvalidate_I, 0(t1)
		cache	IndexInvalidate_I, 32(t1)
		cache	IndexInvalidate_I, 64(t1)
		cache	IndexInvalidate_I, 96(t1)
	
		addu	t1, t0				# step to set F
		cache	IndexInvalidate_I, 0(t1)
		cache	IndexInvalidate_I, 32(t1)
		cache	IndexInvalidate_I, 64(t1)
		cache	IndexInvalidate_I, 96(t1)
	
		addu	t1, t0				# step to set G
		cache	IndexInvalidate_I, 0(t1)
		cache	IndexInvalidate_I, 32(t1)
		cache	IndexInvalidate_I, 64(t1)
		cache	IndexInvalidate_I, 96(t1)
	
		addu	t1, t0				# step to set H
		cache	IndexInvalidate_I, 0(t1)
		cache	IndexInvalidate_I, 32(t1)
		cache	IndexInvalidate_I, 64(t1)
		cache	IndexInvalidate_I, 96(t1)
	
	3:
		cache	IndexInvalidate_I, 0(a0)	# do set (A if NWay)
		cache	IndexInvalidate_I, 32(a0)
		cache	IndexInvalidate_I, 64(a0)
		cache	IndexInvalidate_I, 96(a0)
	
		bne a1, zero, 1b
		addu	a0, 128
	
		j	ra
		addu	v0, zero,zero		# suiword depends on this!!
	END(CPU_FlushICache)





		static char loadelf_buf[2048];
		
		long md_cachestat(void);
#define ICACHE  1
#define DCACHE  2
#define	whatcpu	0
#define IADDR   4
		
		typedef int 		   register_t;
		typedef int 		 f_register_t;
		
		struct trapframe {
			register_t	zero;
			register_t	ast;
			register_t	v0;
			register_t	v1;
			register_t	a0;
			register_t	a1;
			register_t	a2;
			register_t	a3;
			register_t	t0;
			register_t	t1;
			register_t	t2;
			register_t	t3;
			register_t	t4;
			register_t	t5;
			register_t	t6;
			register_t	t7;
			register_t	s0;
			register_t	s1;
			register_t	s2;
			register_t	s3;
			register_t	s4;
			register_t	s5;
			register_t	s6;
			register_t	s7;
			register_t	t8;
			register_t	t9;
			register_t	k0;
			register_t	k1;
			register_t	gp;
			register_t	sp;
			register_t	s8;
			register_t	ra;
			register_t	sr;
			register_t	mullo;
			register_t	mulhi;
			register_t	badvaddr;
			register_t	cause;
			register_t	pc;
		
			f_register_t	f0;
			f_register_t	f1;
			f_register_t	f2;
			f_register_t	f3;
			f_register_t	f4;
			f_register_t	f5;
			f_register_t	f6;
			f_register_t	f7;
			f_register_t	f8;
			f_register_t	f9;
			f_register_t	f10;
			f_register_t	f11;
			f_register_t	f12;
			f_register_t	f13;
			f_register_t	f14;
			f_register_t	f15;
			f_register_t	f16;
			f_register_t	f17;
			f_register_t	f18;
			f_register_t	f19;
			f_register_t	f20;
			f_register_t	f21;
			f_register_t	f22;
			f_register_t	f23;
			f_register_t	f24;
			f_register_t	f25;
			f_register_t	f26;
			f_register_t	f27;
			f_register_t	f28;
			f_register_t	f29;
			f_register_t	f30;
			f_register_t	f31;
			register_t	fsr;
		
			register_t	count;
			register_t	compare;
			register_t	watchlo;
			register_t	watchhi;
			register_t	watchm;
			register_t	watch1;
			register_t	watch2;
			register_t	lladr;
			register_t	ecc;
			register_t	cacher;
			register_t	taglo;
			register_t	taghi;
			register_t	wired;
			register_t	pgmsk;
			register_t	entlo0;
			register_t	entlo1;
			register_t	enthi;
			register_t	context;
			register_t	xcontext;
			register_t	index;
			register_t	random;
			register_t	config;
			register_t	icr;
			register_t	ipllo;
			register_t	iplhi;
			register_t	prid;
			register_t	pcount;
			register_t	pctrl;
			register_t	errpc;
		};
		
		
		struct trapframe *cpuinfotab[8];
		extern struct trapframe DBGREG;
		
		void flushicache(void *p, size_t s)
		{
			CPU_FlushICache(CACHED_MEMORY_ADDR, CpuPrimaryInstCacheSize);
		}
		
		static void flush_cache (int type, void *adr)
		{
		
			switch (type) 
			{
			case ICACHE:
				flushicache((void *)0, kmempos);
				break;
		
			case DCACHE:
				flushdcache((void *)0, kmempos);
				break;
		
			case IADDR:
				syncicache((void *)((int)adr & ~3), 4);
				break;
		
			case ICACHE|DCACHE:
				flushcache();
				break;
			}
		}
		
		
		void md_setpc(struct trapframe *tf, register_t pc)
		{
			if (tf == NULL)
				tf = cpuinfotab[whatcpu];
			tf->pc = (int)pc;
		}
		
		
		
		
		
		
		




