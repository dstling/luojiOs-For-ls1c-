#include <stdio.h>
#include <ls1c_mipsregs.h>
#include <ls1c_irq.h>

#include "rtdef.h"
#include <shell.h>
extern struct ls1c_intc_regs volatile *ls1c_hw0_icregs ;
#define MAX_INTR            (LS1C_NR_IRQS)
//#define RT_NAME_MAX 10

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
	printf("tlb-miss happens, epc: 0x%08x\n", read_c0_epc());
	
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

//=================================================================================================
extern char end_rom[];//系统编译完成后确定 变量在链接脚本ld.script中定义
extern int kmempos;

void shell_thread_entry(void *parameter);
void idle_thread(void*userdata);//空闲线程 做一些死掉的线程清理工作

int osMain(void)
{
	rt_components_init();//组件初始化 优先运行 我们这里什么也没做
	
	rt_system_heap_init(end_rom,(void*)0x81000000);//end+1024*1024*5 堆初始化 malloc可以使用了

	kmempos=0x01100000;//在物理内存11M的位置存储load elf的数据
		
	ls1x_spi_probe(rt_system_heap_init);

	init_fs_netfs();
	init_netfs_tftp();
	
	///*
	sys_tick_init(RT_TICK_PER_SECOND);

	envinit();
	
	uart2GetCharInit();//termio初始化 getchar函数才可以使用

//========================所有thread加入以及初始化======================	
	thread_init();//线程调度初始化
	
	shellInit();//shell初始化

	thread_join_init("idle",idle_thread,NULL,2048,RT_THREAD_PRIORITY_MAX-1,8);//至少加入一个空闲线程，系统管理用 给它倒数第1的优先级

	thread_join_init("shell",shell_thread_entry,NULL,2048,RT_THREAD_PRIORITY_MAX-1,8);//加入shell线程 给他最低的优先级吧 RT_THREAD_PRIORITY_MAX-1
	
	rt_hw_eth_init();//以线程运行的方式初始化 

	list_thread();//显示线程

	thread_start();//开启线程运作 永不回头了
	//*/
	
	//event_callback

	//while(1)
	//	printf("tick:%d\n",sys_tick_get());
	return 0;
}
