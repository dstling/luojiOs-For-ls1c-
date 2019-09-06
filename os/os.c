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
	//printf("rt_interrupt_enter\n");
	rt_interrupt_nest++;

}

void rt_interrupt_leave(void)
{
    rt_interrupt_nest --;
}

void show_irq_nest(void)//打印中断嵌套数值 >1说明中断嵌套了 1说明处在禁止中断状态
{
	printf("rt_interrupt_nest:%d\n",rt_interrupt_nest);
}
FINSH_FUNCTION_EXPORT(show_irq_nest,show_irq_nest in sys);

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

int osMain(void)
{
	rt_components_init();//组件初始化 优先运行 我们这里什么也没做
	
	init_heaptop();//堆初始化 malloc可以使用了

	//test_malloc();
	//return 0;
	
	///*
	sys_tick_init(RT_TICK_PER_SECOND);
	
	uart2GetCharInit();//termio初始化 getchar函数才可以使用

//========================所有thread加入以及初始化======================	
	thread_init();//线程调度初始化
	
	shellInit();//shell线程
	
	//rt_hw_eth_init();//以线程运行的方式初始化 tcpip初始化用到了信号量wait等操作
	
	//testThread();//其他测试线程
	//sem_testThread();
	//deadThread_jointhread();

	ps_thread();//显示线程
	
	thread_start();//开启线程运作 永不回头了
	//*/

	//while(1)
	//	printf("tick:%d\n",sys_tick_get());
	return 0;
}
