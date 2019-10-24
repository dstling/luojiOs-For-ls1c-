// 系统滴答定时器相关接口


#include "ls1c_mipsregs.h"


#define CPU_HZ			                (252 * 1000000)


static volatile unsigned long system_tick;


// 禁止滴答定时器
void sys_tick_disable(void)
{
    unsigned int status = 0;

    status = read_c0_status();
    status &= (~STATUSF_IP7);
    write_c0_status(status);

    return ;
}


// 使能滴答定时器
void sys_tick_enable(void)
{
    unsigned int status = 0;

    status = read_c0_status();
    status |= STATUSF_IP7;
    write_c0_status(status);

    return ;
}


// 滴答定时器初始化
void sys_tick_init(unsigned int tick)
{
    // 设置定时时间
    write_c0_compare(CPU_HZ/2/tick);
    write_c0_count(0);//当count值到达compare时发生一次中断

    // 使能
    sys_tick_enable();

    return ;
}


void sys_tick_increase(void)
{
    ++system_tick;
}

#include<rtdef.h>
#include<stdio.h>
extern void rt_schedule(void);
extern struct rt_thread *rt_current_thread;

// 滴答定时器中断处理函数
void sys_tick_handler(void)
{
    unsigned int count;

    count = read_c0_compare();
    write_c0_compare(count);
    write_c0_count(0);

    sys_tick_increase();
	
	//threadTask();//dstling
	if(rt_current_thread!=NULL)
	{
		--rt_current_thread->remaining_tick;
		if(rt_current_thread->remaining_tick==0)//当前运行线程tick用完了
		{	
			rt_current_thread->remaining_tick=rt_current_thread->init_tick;//重新赋值
			rt_schedule();//执行一次调度
		}
		
		rt_timer_check();
	}
    return ;
}


// 获取tick值
unsigned long sys_tick_get(void)
{
    return system_tick;
}




