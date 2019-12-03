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

extern struct rt_thread *rt_current_thread;

#define LINESZ	256		/* max command line length */
#define MAX_AC	100		/* max number of args to commands */

extern unsigned int rt_interrupt_nest;
extern unsigned int reserve_mem_space;

#define loadStackSize 	(128*8*4)//(4096*10)
extern long LOAD_THREAD;		//在context_gcc.S中定义 原因是load的时候地址靠前安全
extern long LOAD_THREAD_STACK;	//在context_gcc.S中定义

void* loadEntry;
static char loadelf_buf[2048];

rt_thread_t go_thread=NULL;
unsigned char* loadStack;

char	clientcmd[LINESZ];
char	*clientav[MAX_AC];
int		clientac;

int argvize(char *av[], char *s)
{
	char **pav = av, c;
	int ac;

	for (ac = 0; ac < MAX_AC; ac++) 
	{
		/* step over cntrls and spaces */
		while (*s && *s <= ' ')
			s++;

		if (!*s)
			break;

		c = *s;
		/* if it's a quote skip forward */
		if (c == '\'' || c == '"') 
		{
			if (pav)
				*pav++ = ++s;
			while (*s && *s != c)
				s++;
			if (*s)
				*s++ = 0;
		} 
		else 
		{		/* find end of word */
			if (pav)
				*pav++ = s;
			while (' ' < *s)
				s++;
		}

		if (*s)
			*s++ = 0;
	}
	return (ac);
}

int errinval(void)
{
	return(-1);
}

pcireg_t _pci_conf_readn(pcitag_t tag, int reg, int width)
{
	printf("_pci_conf_readn###############\n");
	return 1;
}

void _pci_conf_writen(pcitag_t tag, int reg, pcireg_t data, int width)
{
	printf("_pci_conf_writen###############\n");
}

pcitag_t _pci_make_tag(int bus, int device, int function)
{
	pcitag_t tag;
	printf("_pci_make_tag###############\n");
	tag = (bus << 16) | (device << 11) | (function << 8);
	return(tag);
}

int printf (const char *fmt, ...);
char * gets (char *p);
void CPU_FlushCache(void);//在cache.S中定义

struct callvectors {
	int	(*open) (const char *, int, ...);
	int	(*close) (int);
	int	(*read) (int, void *, size_t);
	int	(*write) (int, const void *, size_t);
	off_t	(*lseek) (int, off_t, int);
	int	(*printf) (const char *, ...);
	void	(*flushcache) (void);
	char	*(*gets) (char *);
#if defined(SMP)
	int	(*smpfork) (size_t, char *);
	int	(*semlock) (int);
	void	(*semunlock) (int);
#else
	int	(*smpfork) (void);
	int	(*semlock) (void);
	int	(*semunlock) (void);
#endif
	unsigned int 	(*_pci_conf_readn)(unsigned long, int, int);
	void	(*_pci_conf_writen)(unsigned long, int, unsigned int, int);
	unsigned long 	(*_pci_make_tag) (int, int, int); 
} callvectors = {
	open,
	close,
	read,
	write,
	lseek,
	printf,
	CPU_FlushCache,
	gets,
	errinval,
	errinval,
	errinval,
	_pci_conf_readn,
	_pci_conf_writen,
	_pci_make_tag,
};
struct callvectors *callvec = &callvectors;

//============================================================================
//Return status of caches (on or off)
int md_cachestat(void)
{
	return 1;
}

void do_loadelf(void*userdata)
{
	int offsetN=0;
	long ep;
	char* loadelfAddr=getenv("loadelfAddr");
	if(loadelfAddr!=NULL)
	{
		int fd=open(loadelfAddr,0|O_NONBLOCK);//mode  OpenLoongsonLib1c.bin
		dl_initialise(0, 0);
		printf("loading...fd:%d\n",fd);
		if(fd>-1)
		{
			ep=load_elf(fd,loadelf_buf,&offsetN,0);
			printf ("\nEntry address is %08x\n", ep);
			close(fd);
			loadEntry=ep;//指定load kernel入口地址
			dl_setloadsyms(md_getpc(NULL));
		}
		else
			printf("tftp Open error.\n");
	}
	else
		printf("set loadelfAddr tftp://x.x.x.x/elfFormatFilename");
	
	THREAD_DEAD_EXIT;
}

#define TROFF(x) ((int)&(((struct threadframe *)0)->x) / sizeof(register_t))
const struct RegList {
	int	regoffs;	/* Location where register is stored */
	int regmap;	/* Bit map for unpacking register info *///const struct RegMap *
	const char	*regname;	/* Register name */
	const char	*regaltname;	/* Alternate register name */
	const int	flags;		/* Register status flags */
#define R_GPR	0x00010000			/* General purpose reg */
#define	R_64BIT	0x00020000			/* Width can be 64 bits */
#define	R_RO	0x00040000			/* Read only register */
#define	R_FLOAT	0x00080000			/* Floating point register */
#define CPU_ALL		0x000f		/* Belongs to mask */
} reglist[] = {
    {TROFF(zero),     0,        "zero", "0",     CPU_ALL | R_GPR | R_64BIT | R_RO},
    {TROFF(at),       0,        "at",   "1",     CPU_ALL | R_GPR | R_64BIT},
    {TROFF(v0),       0,        "v0",   "2",     CPU_ALL | R_GPR | R_64BIT},
    {TROFF(v1),       0,        "v1",   "3",     CPU_ALL | R_GPR | R_64BIT},
    {TROFF(a0),       0,        "a0",   "4",     CPU_ALL | R_GPR | R_64BIT},
    {TROFF(a1),       0,        "a1",   "5",     CPU_ALL | R_GPR | R_64BIT},
    {TROFF(a2),       0,        "a2",   "6",     CPU_ALL | R_GPR | R_64BIT},
    {TROFF(a3),       0,        "a3",   "7",     CPU_ALL | R_GPR | R_64BIT},
    {TROFF(t0),       0,        "t0",   "8",     CPU_ALL | R_GPR | R_64BIT},
    {TROFF(t1),       0,        "t1",   "9",     CPU_ALL | R_GPR | R_64BIT},
    {TROFF(t2),       0,        "t2",   "10",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(t3),       0,        "t3",   "11",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(t4),       0,        "t4",   "12",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(t5),       0,        "t5",   "13",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(t6),       0,        "t6",   "14",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(t7),       0,        "t7",   "15",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(s0),       0,        "s0",   "16",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(s1),       0,        "s1",   "17",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(s2),       0,        "s2",   "18",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(s3),       0,        "s3",   "19",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(s4),       0,        "s4",   "20",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(s5),       0,        "s5",   "21",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(s6),       0,        "s6",   "22",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(s7),       0,        "s7",   "23",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(t8),       0,        "t8",   "24",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(t9),       0,        "t9",   "25",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(k0),       0,        "k0",   "26",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(k1),       0,        "k1",   "27",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(gp),       0,        "gp",   "28",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(sp),       0,        "sp",   "29",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(s8),       0,        "s8",   "30",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(ra),       0,        "ra",   "31",    CPU_ALL | R_GPR | R_64BIT},
		
    {TROFF(sr),       0,  		"sr",   "32",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(mullo),    0,        "lo",   "33",    CPU_ALL | R_GPR | R_64BIT},
    {TROFF(mulhi),    0,        "hi",   "34",    CPU_ALL | R_GPR | R_64BIT},
	{TROFF(badvaddr), 0, 		"badvaddr","35", CPU_ALL | R_GPR | R_64BIT},
	{TROFF(cause),    0,	 	"cause","36", 	 CPU_ALL | R_GPR | R_64BIT},
    {TROFF(pc),       0,        "epc",  "37",    CPU_ALL | R_GPR | R_64BIT},
    {-1}
};
	
void dsp_rregs(struct threadframe *tf)
{
	int i;
	char *hf, *vf;
	char *hp, *vp;
	char values[100];
	char prnbuf[LINESZ + 8]; /* commonly used print buffer */
	hf = " %~8s";
	vf = " %08x";
	hp = prnbuf;
	vp = values;

	for(i = 0; reglist[i].regoffs >= 0; i++) 
	{
		if(!(reglist[i].flags & R_GPR) || !(reglist[i].flags & 1)) 
		{
			continue;
		}
		hp += sprintf(hp, hf, reglist[i].regname);
		vp += sprintf(vp, vf,*((register_t *)tf + reglist[i].regoffs));
		if((vp - values) > 65) 
		{
			printf("%s\n",prnbuf);
			sprintf(prnbuf, "%s", values);
			printf("%s\n",prnbuf);
			hp = prnbuf;
			vp = values;
		}
	}
	
	if(vp > values) 
	{
		printf("%s\n",prnbuf);
		sprintf(prnbuf, "%s", values);
		printf("%s\n",prnbuf);
	}
}

void loadelf_exit(void)
{
	while(1)
		printf("loadelf_exit.\n");
}

void go_loaded_elf(void)
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
			irq_disable_all();
			rt_current_thread=NULL;//无论如何要停止线程调度
		}
		//*/
		loadStack=&LOAD_THREAD_STACK;	//指定load的地址使用的stack 在context_gcc.S中定义
		go_thread=&LOAD_THREAD;			//在context_gcc.S中定义
		memset(loadStack,0,loadStackSize);

		go_thread->sp=rt_hw_stack_init(loadEntry,NULL,loadStack+loadStackSize-sizeof(long),loadelf_exit,go_thread);
		dl_setloadsyms(go_thread->tf->pc);
		go_thread->tf->gp=0;
			
		load_debug("loadStack:0x%08x 2:0x%08x\n",loadStack,loadStack+loadStackSize-sizeof(long));
		load_debug("go_thread->tf->sp:0x%08x\n",go_thread->tf->sp);
		//在stack中设置下a0~a3的参数 cmdline传入load的目标参数等
		{
			char	**vsp, *ssp;
			int ec, stringlen, vectorlen, stacklen, i;
			register_t nsp;
			ec=stringlen=0;
			envsize (&ec, &stringlen);
			for (i = 0; i < clientac; i++) 
			{
				stringlen += strlen(clientav[i]) + 1;
			}
			stringlen = (stringlen + 3) & ~3;					// Round to words 总共有多少个字符 4字节对齐
			vectorlen = (clientac + ec + 2) * sizeof (char *);	//总共有多少个参数+2
			stacklen = ((vectorlen + stringlen) + 7) & ~7;		//8字节对齐
			
			printf("stringlen:%d,vectorlen:%d,stacklen:%d\n",stringlen,vectorlen,stacklen);

			char *argv=malloc(stacklen);
			nsp=argv;
			memset(argv,0,stacklen);
			//nsp=go_thread->sp-stacklen;//向下增长stack深度,用于存储传递的参数
			//nsp=go_thread->tf->sp-stacklen;//向下增长stack深度,用于存储传递的参数
			
			go_thread->tf->a0=clientac;//参数个数
			go_thread->tf->a1=((int)nsp)&0xfffffff|0xa0000000;//参数位置
			go_thread->tf->a2=((int)(nsp + (clientac + 1) * sizeof(char *)))&0xfffffff|0xa0000000;
			go_thread->tf->a3=(int)callvec;
			go_thread->tf->sr=0x24000002;
			//go_thread->tf->sp=((int)go_thread->tf->sp)&0xfffffff|0xa0000000;
			
			go_thread->tf->sp=go_thread->tf->a1-32;//继续向下增长stack深度
			memset((void *)((int)nsp-32), 0, 32);
			load_debug("go_thread->tf->sp:0x%08x,nsp0x%08x\n",go_thread->tf->sp,nsp);
			
			vsp = (char **)(int)nsp;
			ssp = (char *)((int)nsp + vectorlen);
			
			for (i = 0; i < clientac; i++)
			{
				*vsp++ = ssp;
				strcpy (ssp, clientav[i]);
				ssp += strlen(clientav[i]) + 1;
			}
			*vsp++ = (char *)0;

			if(ec)
				envbuild (vsp, ssp);
		}
		
		//printf_c0_status();
		load_debug("loadEntry:0x%08x\n",loadEntry);
		load_debug("loadStack:0x%08x\n",loadStack);
		load_debug("rt_interrupt_nest:%d,go_thread->sp:0x%08x\n",rt_interrupt_nest,go_thread->sp);

		dsp_rregs(go_thread->tf);//显示thread frame状态
		if(md_cachestat())//都在cache.S中定义
			CPU_FlushCache();//flush_cache(DCACHE | ICACHE, NULL);
		CPU_SetSR(0, SR_BOOT_EXC_VEC);
		//CPU_DisableCache();
		//转入load内核 下面是生是死和luojios无关了
		rt_hw_context_switch_to((unsigned long)&go_thread->sp);
		//_go();//在context_gcc.S中定义
		
		/*
		if(rt_interrupt_nest==0)
		{
			rt_hw_context_switch((unsigned long)&from_thread->sp,(unsigned long)&go_thread->sp);
			return;
		}
		else
		{
			rt_hw_context_switch_interrupt((unsigned long)&from_thread->sp,(unsigned long)&go_thread->sp);
		}
		//*/
		printf("go_loaded_elf end \n");
	}
	
}

void newgo_loaded_elf(void);
void g(int argc,char**argv)
{
	int optind=0;
	char buf[30];
	char *mtdparts,*append;
	memset(clientcmd,0,LINESZ);
	while (optind < argc) 
	{
		strcat (clientcmd, argv[optind++]);
		strcat (clientcmd, " ");
	}
	
	append=getenv("append");
	{
		load_debug("append = %s\n",append);
		strcat(clientcmd, append);
		strcat (clientcmd, " ");
	}
	
	mtdparts=getenv("mtdparts");
	if(mtdparts) 
	{
		sprintf(buf,"mtdparts=%s ",mtdparts);
		strcat(clientcmd, buf);
	}
	
	load_debug("g,clientcmd:%s\n",clientcmd);

	clientac = argvize(clientav, clientcmd);//整理传入的参数列表

#ifdef LOAD_DEBUG
	int indx=0;
	for(indx=0;indx<clientac;indx++)
	{
		load_debug("argv[%d]:%s\n",indx,clientav[indx]);
	}
#endif

	//thread_join_init("do_go_loaded_elf",newgo_loaded_elf,NULL,4096*10,0,100);//使用new_go.c中的代码
	thread_join_init("do_go_loaded_elf",go_loaded_elf,NULL,4096*10,0,100);
}
FINSH_FUNCTION_EXPORT(g,go load kernel);

void loadelf(int argc,char**argv)
{
	int prio=TCPIP_THREAD_PRIO_GLO+4;
	int thisTick=100;
	char openDevStr1[]="/dev";
	char* loadelfAddr=getenv("loadelfAddr");
	if(strncmp(openDevStr1,loadelfAddr,strlen(openDevStr1))==0)
	{
		prio=0;//如果打开的是本地设备优先级直接最高
		thisTick=5000;
	}
	printf("loadelf prio:%d,thisTick:%d\n",prio,thisTick);
	thread_join_init("do_loadelf",do_loadelf,NULL,4096*10,prio,thisTick);//load网络内核 线程优先级要低于tcpip线程
}
FINSH_FUNCTION_EXPORT(loadelf,loadelf tftp addr);


extern struct threadframe *cpuinfotab[];
void newgo_loaded_elf2(void)
{
	int optind=0;
	char buf[30];
	char *mtdparts,*append;
	memset(clientcmd,0,LINESZ);
	/*
	while (optind < argc) 
	{
		strcat (clientcmd, argv[optind++]);
		strcat (clientcmd, " ");
	}
	*/
	strcat (clientcmd, "g ");
	
	append=getenv("append");
	{
		load_debug("append = %s\n",append);
		strcat(clientcmd, append);
		strcat (clientcmd, " ");
	}
	
	mtdparts=getenv("mtdparts");
	if(mtdparts) 
	{
		sprintf(buf,"mtdparts=%s ",mtdparts);
		strcat(clientcmd, buf);
	}
	
	load_debug("g,clientcmd:%s\n",clientcmd);

	clientac = argvize(clientav, clientcmd);//整理传入的参数列表

#ifdef LOAD_DEBUG
	int indx=0;
	for(indx=0;indx<clientac;indx++)
	{
		load_debug("argv[%d]:%s\n",indx,clientav[indx]);
	}
#endif



	rt_thread_t from_thread=rt_current_thread;
	if(loadEntry!=0)
	{
		///*
		register long level;
		if(rt_current_thread!=NULL)//说明此时线程已经运作
		{
			level=rt_hw_interrupt_disable();
			//sys_tick_disable();
			irq_disable_all();
			rt_current_thread=NULL;//无论如何要停止线程调度
		}
		//*/
		md_setpc(NULL, loadEntry);
		
		initstack (clientac, clientav, 1);
		if(md_cachestat())//都在cache.S中定义
			CPU_FlushCache();//flush_cache(DCACHE | ICACHE, NULL);
		md_setsr(NULL, 0x24000002);
		
		dsp_rregs(cpuinfotab[whatcpu]);//显示thread frame状态
		
		tgt_enable (tgt_getmachtype ()); /* set up i/u hardware 基本上也是啥事没做*/


		_go ();

		
	}
}


