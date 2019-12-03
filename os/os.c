#include <buildconfig.h>
#include <stdio.h>
#include <ls1c_mipsregs.h>
#include <ls1c_irq.h>
#include <cpu.h>

#include "rtdef.h"
#include <shell.h>
extern struct ls1c_intc_regs volatile *ls1c_hw0_icregs ;
#define MAX_INTR            (LS1C_NR_IRQS)
//#define RT_NAME_MAX 10
extern int kmempos;
#define UNCACHED_MEMORY_ADDR    0xa0000000
#define PHYS_TO_UNCACHED(x)     ((unsigned)(x) | UNCACHED_MEMORY_ADDR)

unsigned int rt_thread_switch_interrupt_flag=0;
unsigned int rt_interrupt_from_thread=0;
unsigned int rt_interrupt_to_thread=0;
unsigned int rt_interrupt_nest=0;

void rt_interrupt_enter(void)
{
    long level = rt_hw_interrupt_disable();
	rt_interrupt_nest++;
    rt_hw_interrupt_enable(level);

}

void rt_interrupt_leave(void)
{
    long level = rt_hw_interrupt_disable();
    rt_interrupt_nest --;
    rt_hw_interrupt_enable(level);
}


RT_WEAK unsigned char rt_interrupt_get_nest(void)
{
    unsigned char ret;
    long level;

    level = rt_hw_interrupt_disable();
    ret = rt_interrupt_nest;
    rt_hw_interrupt_enable(level);
    return ret;
}

void rt_hw_interrupt_umask(int vector)//开启中断
{
    (ls1c_hw0_icregs+(vector>>5))->int_en |= (1 << (vector&0x1f));
}

void rt_hw_interrupt_mask(int vector)//关闭中断
{
    /* mask interrupt */
    (ls1c_hw0_icregs+(vector>>5))->int_en &= ~(1 << (vector&0x1f));
}

void rt_hw_cpu_shutdown(void)
{
	printf("shutdown...\n");
	while (1);
}

void tlb_refill_handler(void)
{
	printf("tlb-miss happens,epc: 0x%08x\n", read_c0_epc());
	printf("tlb-miss happens,CAUSE: 0x%08x\n", read_c0_cause());
	printf("tlb-miss happens,STATU: 0x%08x\n", read_c0_status());
	printf("tlb-miss happens,BAD_VADDR: 0x%08x\n", read_c0_badvaddr());
	
	list_thread();
	rt_hw_cpu_shutdown();
}

//=================================下面这些是必须的============================================
static int rti_start(void)
{
    return 0;
}
INIT_EXPORT(rti_start, "0");

static int rti_board_start(void)
{
    return 0;
}
INIT_EXPORT(rti_board_start, "0.end");

static int rti_board_end(void)
{
    return 0;
}
INIT_EXPORT(rti_board_end, "1.end");

static int rti_end(void)
{
    return 0;
}
INIT_EXPORT(rti_end, "6.end");

void rt_components_init(void)
{
    const init_fn_t *fn_ptr;

    for (fn_ptr = &__rt_init_rti_board_end; fn_ptr < &__rt_init_rti_end; fn_ptr ++)
    {
        (*fn_ptr)();
    }
}

static inline void sync(void)//就是一句空话 在start.s中被调用一次 wbflush
{
}

//typedef	unsigned int u_int;

u_int	CpuPrimaryInstCacheSize;
u_int	CpuPrimaryInstCacheLSize;
u_int	CpuPrimaryInstSetSize;
u_int	CpuPrimaryDataCacheSize;
u_int	CpuPrimaryDataCacheLSize;
u_int	CpuPrimaryDataSetSize;
u_int	CpuCacheAliasMask;
u_int	CpuSecondaryCacheSize;
u_int	CpuTertiaryCacheSize;
u_int	CpuNWayCache;
u_int	CpuNWayICache;
u_int	CpuNWayDCache;
u_int	CpuCacheType;		// R4K, R5K, RM7K 
u_int	CpuConfigRegister;
u_int	CpuStatusRegister;
u_int	CpuExternalCacheOn;	// R5K, RM7K 
u_int	CpuOnboardCacheOn;	// RM7K 

union cpuprid {
	int	cpuprid;
	struct {
		unsigned int	cp_minrev:4;	/* minor revision identifier */
		unsigned int	cp_majrev:4;	/* major revision identifier */
		unsigned int	cp_imp:8;	/* implementation identifier */
		unsigned int	pad1:16;	/* reserved */
	} cpu;
};
union	cpuprid CpuProcessorId;

void tgt_cache_print(void)
{
	printf("Primary Instruction cache size %dkb (%d line, %d way)\n",
		CpuPrimaryInstCacheSize / 1024, CpuPrimaryInstCacheLSize, CpuNWayCache);
	
	printf("Primary Data cache size %dkb (%d line, %d way)\n",
		CpuPrimaryDataCacheSize / 1024, CpuPrimaryDataCacheLSize, CpuNWayCache);

	if (CpuSecondaryCacheSize != 0) 
	{
		printf("Secondary cache size %dkb\n", CpuSecondaryCacheSize / 1024);
	}
	if (CpuTertiaryCacheSize != 0) 
	{
		printf("Tertiary cache size %dkb\n", CpuTertiaryCacheSize / 1024);
	}
}

#define MIPS_GODSON2	0x63	/* Godson 2 CPU */
#define MIPS_GODSON1	0x42    /* Godson 1 CPU */
const char *md_cpuname(void)
{
	unsigned long cpuid=read_c0_prid();

	switch((cpuid >> 8) & 0xff) 
	{
		case MIPS_GODSON2:
			return("GODSON2");
		case MIPS_GODSON1:
#ifdef CPU_NAME
	 		return CPU_NAME;
#endif
			return NULL;
		default:
			return("unidentified");
	}
}
void tgt_machprint(void)
{
	printf("Copyright 2000-2002, Opsycon AB, Sweden.\n");
	printf("Copyright 2005, ICT CAS.\n");
	printf("CPU %s @\n", md_cpuname());
} 
register_t tgt_clienttos(void)
{
	return((register_t)(int)PHYS_TO_UNCACHED(kmempos & ~7) - 64);
}

//=================================================================================================
extern char end_rom[];//系统编译完成后确定 变量在链接脚本ld.script中定义
unsigned int reserve_mem_space=0;
struct threadframe *cpuinfotab[8];
extern long LOAD_THREAD_STACK;	//在context_gcc.S中定义
#define CLIENTPC 0x80100000
extern char MipsException[], MipsExceptionEnd[];

jmp_buf         jmpbMainStart;		/* non-local goto jump buffer */

long flagJmpMain=0;

int osMain(void)
{
	kmempos=0x01100000;//在物理内存11M的位置存储load elf的数据
	/*//使用new_go.c中的代码
	CPU_ConfigCache();
	cpuinfotab[whatcpu] = &LOAD_THREAD_STACK;

	md_setpc(NULL, (int32_t) CLIENTPC);
	md_setsp(NULL, tgt_clienttos());
	
	//setjmp(jmpbMainStart);
	//memcpy((char*)TLB_MISS_EXC_VEC,	MipsException, MipsExceptionEnd - MipsException);
	//memcpy((char*)GEN_EXC_VEC,		MipsException, MipsExceptionEnd - MipsException);

	//CPU_FlushCache();
	//CPU_SetSR(0, SR_BOOT_EXC_VEC);
	//*/
	sys_tick_init(RT_TICK_PER_SECOND);
	reserve_mem_space=0x82000000-1024*1024;
	rt_system_heap_init(0x81b00000,0x81f00000);//end+1024*1024*5 堆初始化 malloc可以使用了 end_rom,(void*)0x81000000
	ls1x_spi_probe();
	uart2GetCharInit();//串口初始化 getchar函数可以使用
	envinit();
	dc_init();
	rt_components_init();//组件初始化 优先运行 我们这里什么也没做
	
	tgt_cache_print();
	tgt_machprint();
	
	init_fs_netfs();
	init_netfs_tftp();
	init_fs_fat();
	init_fs_mtdfs();

//========================所有thread加入以及初始化======================	
	thread_init();//线程调度初始化

	shell_init();
	idle_init();
	eth_rt_tx_init();
	rt_hw_eth_init();	//以线程的方式初始化网卡和lwip 
	usb_init();			//以线程的方式初始化和探测usb设备
	nand_init();		//以线程的方式初始化和探测nand设备
	//list_thread();//显示线程

	thread_start();//开启线程运作 永不回头了

	while(1)
	{
		printf("out thread mode,tick:%d\n",sys_tick_get());
		delay_ms(1000);
	}
	return 0;
}
