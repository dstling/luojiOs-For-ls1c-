#include <buildconfig.h>
#include <types.h>
#include <stdio.h>
#include "filesys.h"
#include <shell.h>
#include <rtdef.h>
#include <cpu.h>

#define LOAD_DEBUG
#ifdef LOAD_DEBUG
#define load_debug(format, arg...) printf("LOAD_DEBUG: " format, ## arg)
#else
#define load_debug(format, arg...)
#endif 
#define loadStackSize 	(128*8*4)//(4096*10)

void loadelf_exit(void);

extern struct rt_thread *rt_current_thread;
extern rt_thread_t go_thread;
extern unsigned char* loadStack;
extern void* loadEntry;
extern long LOAD_THREAD;		//在context_gcc.S中定义 原因是load的时候地址靠前安全
extern long LOAD_THREAD_STACK;	//在context_gcc.S中定义
extern struct threadframe *cpuinfotab[];
extern struct callvectors *callvec;

void exception(struct threadframe *frame)
{
	printf("exception\n");
}


/*
void exception(struct threadframe *frame)
{
	int exc_type;
	int i, flag;
	char *p = 0;
	struct threadframe *cpuinfo;

	cpuinfo = cpuinfotab[0];
	*cpuinfo = *frame;		// Save the frame 

	exc_type = md_exc_type(frame);


	if(exc_type != EXC_BPT && exc_type != EXC_TRC && exc_type != EXC_WTCH) 
	{	       	
		if (!p) {
			md_dumpexc(frame);
		}
		pmon_stop (0);
	}
	else if (trace_mode == TRACE_NO) 
	{	// no bpts set 
		printf ("\r\nBreakpoint reached while not in trace mode!\r\n");
		md_fpsave(cpuinfo);
		pmon_stop (0);
	}
	else if (trace_mode == TRACE_GB) 
	{	// go & break 
		if (BptTmp.addr != NO_BPT) 
		{
			store_word ((void *)BptTmp.addr, BptTmp.value);
			BptTmp.addr = NO_BPT;
		}
		for (i = 0; i < MAX_BPT; i++) 
		{
			if (Bpt[i].addr != NO_BPT) 
			{
				store_word ((void *)Bpt[i].addr, Bpt[i].value);
			}
		}
		if (exc_type == EXC_WTCH) 
		{
			printf ("\r\nStopped at HW Bpt %d\r\n", MAX_BPT);
		}
		else 
		{
			for (i = 0; i < MAX_BPT; i++) 
			{
				if(Bpt[i].addr == (int)md_get_excpc(frame)) 
				{
					printf ("\r\nStopped at Bpt %d\n", i);
					md_fpsave(cpuinfo);
					pmon_stop (Bpt[i].cmdstr);
				}
			}
		}
		md_fpsave(cpuinfo);
		pmon_stop (0);
	}

	remove_trace_breakpoint ();
	if (trace_mode == TRACE_TB) 
	{
		md_fpsave(cpuinfo);
		pmon_stop (0);		// trace & break 
	}
	else if (trace_mode == TRACE_TN) 
	{
		for (i = 0; i < MAX_BPT; i++) 
		{
			if(Bpt[i].addr == (int)md_get_excpc(frame)) 
			{
				printf ("\r\nStopped at Bpt %d\r\n", i);
				md_fpsave(cpuinfo);
				pmon_stop (Bpt[i].cmdstr);
			}
		}
		if(trace_invalid && !is_validpc(md_get_excpc(frame))) 
		{
			printf ("\r\nStopped: Invalid PC value\r\n");
			md_fpsave(cpuinfo);
			pmon_stop (0);
		}
		for (i = 0; i < STOPMAX; i++) 
		{
			if (stopval[i].addr == 0) 
			{
				continue;
			}
			if ((stopval[i].sense == 0 &&
				load_word (stopval[i].addr) == stopval[i].value)
				|| (stopval[i].sense == 1 &&
				load_word (stopval[i].addr) != stopval[i].value)) 
			{
				if (stopval[i].sense == 0) 
				{
					p = " == ";
				}
				else 
				{
					p = " != ";
				}
				if (!strcmp(stopval[i].name, "MEMORY")) 
				{
					printf ("\r\nStopped: 0x%08x%s0x%08x\r\n",
						stopval[i].addr, p, stopval[i].value);
				}
				else 
				{
					printf ("\r\nStopped: %s%s0x%08x\r\n", stopval[i].name,p, stopval[i].value);
				}
				md_fpsave(cpuinfo);
				pmon_stop (0);
			}
		}
		flag = 1;
		if (trace_bflag || trace_cflag) 
		{
			if(trace_bflag && md_is_branch(md_get_excpc(frame))) 
			{
				flag = 1;
			}
			else if(trace_cflag && md_is_call(md_get_excpc(frame))) 
			{
				flag = 1;
			}
			else 
			{
				flag = 0;
			}
		}
		if (flag) 
		{
			addpchist (md_get_excpc(frame));
			if (trace_verbose) {
#if NCMD_L > 0
				char tmp[80];
				md_disasm (tmp, md_get_excpc(frame));
				printf ("%s\r\n", tmp);
#endif 
#ifdef HAVE_DLYSLOT
				if (md_is_branch(md_get_excpc(frame))) 
				{
				// print the branch delay slot too 
					md_disasm(tmp, md_get_excpc(frame) + 4);
					printf ("%s\r\n", tmp);
				}
#endif
			}
			else 
			{
				dotik (256, 1);
			}
		}
		else 
		{
			dotik (256, 1);
		}
		if (trace_count) 
		{
			trace_count--;
		}
		if (trace_count == 1) 
		{
			trace_mode = TRACE_TB;
		}
		if (setTrcbp (md_get_excpc(frame), trace_over)) 
		{
			md_fpsave(cpuinfo);
			pmon_stop (0);
		}
		store_trace_breakpoint ();
		console_state(2);
		_go ();
	}
// else TRACE_TG  trace & go, set on g or c if starting at bpt 

	trace_mode = TRACE_GB;	// go & break 
	store_breakpoint ();
	console_state(2);
	_go ();
}
//*/



register_t md_adjstack(struct threadframe *tf, register_t newsp)
{
	register_t oldsp;

	if (tf == NULL)
		tf = cpuinfotab[whatcpu];
	oldsp = tf->sp;
	if(newsp != 0) {
		tf->sp = newsp;
	}
	return(oldsp);
}
void md_setargs(struct threadframe *tf, register_t a1, register_t a2,register_t a3, register_t a4)
{
	if (tf == NULL)
		tf = cpuinfotab[whatcpu];
	tf->a0 = a1;
	tf->a1 = a2;
	tf->a2 = a3;
	tf->a3 = a4;
}

void md_setlr(struct threadframe *tf, register_t lr)
{
	if (tf == NULL)
		tf = cpuinfotab[whatcpu];
	tf->ra = lr;
}
void md_setsr(struct threadframe *tf, register_t sr)
{
	if (tf == NULL)
		tf = cpuinfotab[whatcpu];
	tf->sr = sr;
}
void md_setpc(struct threadframe *tf, register_t pc)
{
	if (tf == NULL)
		tf = cpuinfotab[whatcpu];
	tf->pc = (int)pc;
}
void md_setsp(struct threadframe *tf, register_t sp)
{
	if (tf == NULL)
		tf = cpuinfotab[whatcpu];
	tf->sp = sp;
}
void *md_getpc(struct threadframe *tf)
{
	if (tf == NULL)
		tf = cpuinfotab[whatcpu];
	return((void *)(int)tf->pc);
}

register_t tgt_enable(int machtype)
{
	/* XXX Do any HW specific setup */
	return(SR_COP_1_BIT|SR_FR_32|SR_EXL);
}

int tgt_getmachtype(void)
{
	return(md_cputype());
}

void initstack (int ac, char **av, int addenv)
{
	char	**vsp, *ssp;
	int	ec, stringlen, vectorlen, stacklen, i;
	register_t nsp;

	/*
	 *  Calculate the amount of stack space needed to build args.
	 */
	stringlen = 0;
	if (addenv) 
	{
		envsize (&ec, &stringlen);
	}
	else 
	{
		ec = 0;
	}
	for (i = 0; i < ac; i++) 
	{
		stringlen += strlen(av[i]) + 1;
	}
	stringlen = (stringlen + 3) & ~3;	/* Round to words *///总共有多少个字符
	vectorlen = (ac + ec + 2) * sizeof (char *);//总共有多少个参数+2
	stacklen = ((vectorlen + stringlen) + 7) & ~7;//8字节对齐

	printf("stringlen:%d,vectorlen:%d,stacklen:%d\n",stringlen,vectorlen,stacklen);
	/*
	 *  Allocate stack and use md code to set args.
	 */
	nsp = md_adjstack(NULL, 0) - stacklen;//向下增长stack深度
	printf("ac:0x%08x,nsp:0x%08x,nsp + (ac + 1) * sizeof(char *):0x%08x,(int)callvec:0x%08x\n",ac,nsp,nsp + (ac + 1) * sizeof(char *),(int)callvec);
	md_setargs(NULL, ac, nsp, nsp + (ac + 1) * sizeof(char *), (int)callvec);//设置参数a0~a3

	/* put $sp below vectors, leaving 32 byte argsave */
	md_adjstack(NULL, nsp - 32);
	memset((void *)((int)nsp - 32), 0, 32);

	/*
	 * Build argument vector and strings on stack.
	 * Vectors start at nsp; strings after vectors.
	 */
	vsp = (char **)(int)nsp;
	ssp = (char *)((int)nsp + vectorlen);
	printf("vsp:0x%08x,ssp:0x%08x\n",vsp,ssp);
	for (i = 0; i < ac; i++)
	{
		*vsp++ = ssp;
		strcpy (ssp, av[i]);
		ssp += strlen(av[i]) + 1;
	}
	*vsp++ = (char *)0;

	/* build environment vector on stack */
	if (ec) 
	{
		envbuild (vsp, ssp);
	}
	else 
	{
		*vsp++ = (char *)0;
	}
	/*
	 * Finally set the link register to catch returning programs.
	 */
	md_setlr(NULL, (register_t)loadelf_exit);
}
extern char	clientcmd[];
extern char	*clientav[];
extern int		clientac;
extern char MipsException[], MipsExceptionEnd[];
#define writel(val, addr) (*(volatile unsigned long *)(addr) = (val))
#include <ls1c_mipsregs.h>
#include <ls1c_regs.h>
void newgo_loaded_elf(void)
{
	rt_thread_t from_thread=rt_current_thread;
	if(loadEntry!=0)
	{
		///*
		register long level;
		if(rt_current_thread!=NULL)//说明此时线程已经运作
		{
			level=rt_hw_interrupt_disable();
			//sys_tick_disable();
			//irq_disable_all();
			rt_current_thread=NULL;//无论如何要停止线程调度
		}
		//*/
		md_setpc(NULL, loadEntry);
		
		initstack (clientac, clientav, 1);
		if(md_cachestat())//都在cache.S中定义
			CPU_FlushCache();//flush_cache(DCACHE | ICACHE, NULL);
		md_setsr(NULL, 0x24000002);

		printf_cp0_sr(100);
		
		write_c0_status(0);
		write_c0_cause(0x40008000);
		write_c0_epc(0x00000000);
		write_c0_compare(0x0);
		writel(0x8000549c,LS1C_START_FREQ);
		writel(0xb70082b3,LS1C_CLK_DIV_PARAM);

		
		printf("printf_c0_status happens,epc: 0x%08x\n",	read_c0_epc());
		printf("printf_c0_status,CAUSE: 0x%08x\n",			read_c0_cause());
		printf("printf_c0_status,STATU: 0x%08x\n",			read_c0_status());
		printf("printf_c0_status,BAD_VADDR: 0x%08x\n",		read_c0_badvaddr());
		printf("printf_c0_status,compare: 0x%08x\n",		read_c0_compare());
		printf("printf_c0_status,entrylo0: 0x%08x\n",		read_c0_entrylo0());
		printf("printf_c0_status,entrylo1: 0x%08x\n",		read_c0_entrylo1());
		printf("printf_c0_status,entryhi: 0x%08x\n",		read_c0_entryhi());
		

		dsp_rregs(cpuinfotab[whatcpu]);//显示thread frame状态

		//tgt_enable (tgt_getmachtype ()); /* set up i/u hardware 基本上也是啥事没做*/
/*		
		memcpy( (char *)TLB_MISS_EXC_VEC,MipsException, MipsExceptionEnd - MipsException);
		memcpy((char *)GEN_EXC_VEC,MipsException, MipsExceptionEnd - MipsException);
		
		//CPU_FlushCache();
		//CPU_SetSR(0, SR_BOOT_EXC_VEC);
		dcache_writeback_invalidate_all();
		icache_invalidate_all();
		CPU_SetSR(0, 0x24000002);
		irq_enable_all();
*/

		_go ();

		
	}
}






