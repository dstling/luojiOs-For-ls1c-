
#include <types.h>
#include <stdio.h>

#include "rtdef.h"

extern unsigned int rt_interrupt_nest;


rt_inline void rt_list_init(rt_list_t *l)
{
    l->next = l->prev = l;
}

rt_inline void rt_list_insert_before(rt_list_t *l, rt_list_t *n)
{
    l->prev->next = n;
    n->prev = l->prev;

    l->prev = n;
    n->next = l;
}

rt_inline void rt_list_remove(rt_list_t *n)
{
    n->next->prev = n->prev;
    n->prev->next = n->next;

    n->next = n->prev = n;
}

rt_inline int rt_list_isempty(const rt_list_t *l)
{
    return l->next == l;
}

rt_inline void rt_list_insert_after(rt_list_t *l, rt_list_t *n)
{
	l->next->prev = n;
	n->next = l->next;

	l->next = n;
	n->prev = l;
}

void (*rt_assert_hook)(const char *ex, const char *func, unsigned long line);
void rt_assert_handler(const char *ex_string, const char *func, unsigned long line)
{
    volatile char dummy = 0;

    if (rt_assert_hook == RT_NULL)
    {
        printf("(%s) assertion failed at function:%s, line number:%d \n", ex_string, func, line);
        while (dummy == 0);

    }
    else
    {
        rt_assert_hook(ex_string, func, line);
    }
}

struct rt_thread *rt_current_thread=NULL;
unsigned char rt_current_priority;
rt_list_t rt_thread_priority_table[RT_THREAD_PRIORITY_MAX];
rt_list_t rt_thread_defunct;
rt_list_t rt_all_thread;//所有的线程都在这里

//rt_list_t sleep_list_head;
rt_list_t rt_timer_list;//timer链表

unsigned int rt_thread_ready_priority_group;
//char threadStartFlag=0;
static int scheduleCounterFlag=0;


const unsigned char __lowest_bit_bitmap[] =
{
    /* 00 */ 0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 10 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 20 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 30 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 40 */ 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 50 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 60 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 70 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 80 */ 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 90 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* A0 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* B0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* C0 */ 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* D0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* E0 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* F0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};

register unsigned int $GP __asm__ ("$28");
unsigned char *rt_hw_stack_init(void *tentry, void *parameter, unsigned char *stack_addr, void *texit)
{
	unsigned int *stk;
    static unsigned int g_sr = 0;
	static unsigned int g_gp = 0;

    if (g_sr == 0)
    {
    	g_sr = cp0_get_status();
    	g_sr &= 0xfffffffe;
    	g_sr |= 0x8401;

		g_gp = $GP;
    }

    /** Start at stack top */
    stk = (unsigned int *)stack_addr;
	*(stk)   = (unsigned int) tentry;        /* pc: Entry Point */
	*(--stk) = (unsigned int) 0xeeee; 		/* c0_cause */
	*(--stk) = (unsigned int) 0xffff;		/* c0_badvaddr */
	*(--stk) = (unsigned int) cp0_get_lo();	/* lo */
	*(--stk) = (unsigned int) cp0_get_hi();	/* hi */
	*(--stk) = (unsigned int) g_sr; 			/* C0_SR: HW2 = En, IE = En */
	*(--stk) = (unsigned int) texit;	        /* ra */
	*(--stk) = (unsigned int) 0x0000001e;	/* s8 */
	*(--stk) = (unsigned int) stack_addr;	/* sp */
	*(--stk) = (unsigned int) g_gp;	        /* gp */
	*(--stk) = (unsigned int) 0x0000001b;	/* k1 */
	*(--stk) = (unsigned int) 0x0000001a;	/* k0 */
	*(--stk) = (unsigned int) 0x00000019;	/* t9 */
	*(--stk) = (unsigned int) 0x00000018;	/* t8 */
	*(--stk) = (unsigned int) 0x00000017;	/* s7 */
	*(--stk) = (unsigned int) 0x00000016;	/* s6 */
	*(--stk) = (unsigned int) 0x00000015;	/* s5 */
	*(--stk) = (unsigned int) 0x00000014;	/* s4 */
	*(--stk) = (unsigned int) 0x00000013;	/* s3 */
	*(--stk) = (unsigned int) 0x00000012;	/* s2 */
	*(--stk) = (unsigned int) 0x00000011;	/* s1 */
	*(--stk) = (unsigned int) 0x00000010;	/* s0 */
	*(--stk) = (unsigned int) 0x0000000f;	/* t7 */
	*(--stk) = (unsigned int) 0x0000000e;	/* t6 */
	*(--stk) = (unsigned int) 0x0000000d;	/* t5 */
	*(--stk) = (unsigned int) 0x0000000c;	/* t4 */
	*(--stk) = (unsigned int) 0x0000000b;	/* t3 */
	*(--stk) = (unsigned int) 0x0000000a; 	/* t2 */
	*(--stk) = (unsigned int) 0x00000009;	/* t1 */
	*(--stk) = (unsigned int) 0x00000008;	/* t0 */
	*(--stk) = (unsigned int) 0x00000007;	/* a3 */
	*(--stk) = (unsigned int) 0x00000006;	/* a2 */
	*(--stk) = (unsigned int) 0x00000005;	/* a1 */
	*(--stk) = (unsigned int) parameter;	    /* a0 */
	*(--stk) = (unsigned int) 0x00000003;	/* v1 */
	*(--stk) = (unsigned int) 0x00000002;	/* v0 */
	*(--stk) = (unsigned int) 0x00000001;	/* at */
	*(--stk) = (unsigned int) 0x00000000;	/* zero */

	/* return task's current stack address */
	return (unsigned char *)stk;
}

int __rt_ffs(int value)
{
    if (value == 0) return 0;

    if (value & 0xff)
        return __lowest_bit_bitmap[value & 0xff] + 1;

    if (value & 0xff00)
        return __lowest_bit_bitmap[(value & 0xff00) >> 8] + 9;

    if (value & 0xff0000)
        return __lowest_bit_bitmap[(value & 0xff0000) >> 16] + 17;

    return __lowest_bit_bitmap[(value & 0xff000000) >> 24] + 25;
}

static struct rt_thread* _get_highest_priority_thread(unsigned long *highest_prio)
{
    register struct rt_thread *highest_priority_thread;
    register unsigned long highest_ready_priority;


    highest_ready_priority = __rt_ffs(rt_thread_ready_priority_group) - 1;

    /* get highest ready priority thread */
    highest_priority_thread = rt_list_entry(rt_thread_priority_table[highest_ready_priority].next,
    							struct rt_thread,
								tlist);

    *highest_prio = highest_ready_priority;

    return highest_priority_thread;
}

static void _rt_scheduler_stack_check(struct rt_thread *thread)
{
	if (*((unsigned char *)thread->stack_addr) != '#' ||
		(unsigned long)thread->sp <= (unsigned long)thread->stack_addr ||
		(unsigned long)thread->sp >	(unsigned long)thread->stack_addr + (unsigned long)thread->stack_size)
	{
		unsigned long level;

		printf("thread:%s stack overflow,stack add:0x%08x,sp addr:0x%08x,nest:%d\n", thread->name,thread->stack_addr,thread->sp,rt_interrupt_nest);
		{
			extern long list_thread(void);
			list_thread();
		}
		//level = rt_hw_interrupt_disable();
		//while(1);
	}
}

long rt_timer_stop(rt_timer_t timer)
{
    register long level;

    if (!(timer->flag & RT_TIMER_FLAG_ACTIVATED))
        return -RT_ERROR;


    /* disable interrupt */
    level = rt_hw_interrupt_disable();

    //_rt_timer_remove(timer);
    rt_list_remove(&rt_timer_list);

    /* enable interrupt */
    rt_hw_interrupt_enable(level);

    /* change stat */
    timer->flag = RT_TIMER_FLAG_NO_ACTIVATED;

    return RT_EOK;
}

void rt_thread_exit(void)//线程结束需要做的事情
{
    register long temp = rt_hw_interrupt_disable();
	struct rt_thread *curThread=rt_current_thread;
	
	printf("rt_thread_exit:%s\n",curThread->name);
	
	curThread->statu=RT_THREAD_CLOSE;
	
	rt_timer_stop(&(curThread->thread_timer));
	
	rt_schedule_remove_thread(curThread);
	
	rt_list_insert_after(&rt_thread_defunct, &(curThread->tlist));
	
	rt_hw_interrupt_enable(temp);
	rt_schedule();
}

struct rt_thread *rt_thread_self(void)
{
	return rt_current_thread;
}

void rt_schedule_insert_thread(struct rt_thread *thread)
{
    register long temp = rt_hw_interrupt_disable();

    /* it's current thread, it should be RUNNING thread */
    if (thread == rt_current_thread)
    {
        thread->statu = RT_THREAD_RUNNING;
        return;
    }

    /* READY thread, insert to ready queue */
    thread->statu = RT_THREAD_READY ;
    /* insert thread to ready list */
    rt_list_insert_before(&(rt_thread_priority_table[thread->current_priority]),
                          &(thread->tlist));
    rt_thread_ready_priority_group |= thread->number_mask;

//__exit:
    /* enable interrupt */
    rt_hw_interrupt_enable(temp);
}

void rt_schedule_remove_thread(struct rt_thread *thread)
{
	register long level;

	/* disable interrupt */
	level = rt_hw_interrupt_disable();

	/* remove thread from ready list */
	rt_list_remove(&(thread->tlist));
	if (rt_list_isempty(&(rt_thread_priority_table[thread->current_priority])))
	{
		rt_thread_ready_priority_group &= ~thread->number_mask;
	}
	
    rt_hw_interrupt_enable(level);
}

void rt_timer_init(rt_timer_t        timer,const char *name,void (*timeout)(void *parameter),void *parameter,unsigned int time,char flag)
{
    /* timer check */
    RT_ASSERT(timer != RT_NULL);
    /* timer object initialization */
    //rt_object_init((rt_object_t)timer, RT_Object_Class_Timer, name);

    int i;
    /* set flag */
    //timer->flag  = flag;
    //timer->parent.flag  = flag;

    /* set deactivated */
    //timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;

    /* set deactivated */
    timer->flag =RT_TIMER_FLAG_NO_ACTIVATED;

    timer->timeout_func = timeout;
    timer->parameter    = parameter;

    timer->timeout_tick = 0;
    timer->init_tick    = time;

    /* initialize timer list */
    rt_list_init(&timer->timelist);

}

long rt_timer_start(rt_timer_t timer)
{
	register long level = rt_hw_interrupt_disable();
    timer->timeout_tick = sys_tick_get() + timer->init_tick;//设定超时时间
	
    rt_list_insert_after(&rt_timer_list,&(timer->timelist));//加入timer链表中
    timer->flag = RT_TIMER_FLAG_ACTIVATED;//激活
	
    rt_hw_interrupt_enable(level);
	return RT_EOK;
}



void rt_thread_timeout(void *parameter)//线程timer超时操作 在rt_timer_check中被调用
{
    struct rt_thread *thread;

    thread = (struct rt_thread *)parameter;

    /* thread check */
    RT_ASSERT(thread != RT_NULL);
    //RT_ASSERT((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_SUSPEND);
   // RT_ASSERT(rt_object_get_type((rt_object_t)thread) == RT_Object_Class_Thread);

    /* set error number */
    thread->error = -RT_ETIMEOUT;

    /* remove from suspend list */
    rt_list_remove(&(thread->tlist));

    /* insert to schedule ready list */
    rt_schedule_insert_thread(thread);

    /* do schedule */
    //rt_schedule();//rtthread里面有一次调度
}

void rt_timer_check(void)//定时器超时检测
{
    struct rt_timer *t;
    unsigned int current_tick;
    register long level;

    current_tick = sys_tick_get();

    /* disable interrupt */
    level = rt_hw_interrupt_disable();

	struct rt_timer *timerP,*posTmp;
	rt_list_for_each_entry_safe(timerP,posTmp,&rt_timer_list,timelist)
	{
		if(timerP->flag==RT_TIMER_FLAG_ACTIVATED&&sys_tick_get()>timerP->timeout_tick)
		{
			rt_list_remove(&timerP->timelist);//从timer链表中删除
			timerP->timeout_func(timerP->parameter);//执行超时任务 对于线程来说 就是执行rt_thread_timeout
			timerP->flag =RT_TIMER_FLAG_NO_ACTIVATED;
		}
	}
	
    rt_hw_interrupt_enable(level);

}

unsigned int counterS=0;
void rt_schedule(void)
{
	counterS++;
	register long level = rt_hw_interrupt_disable();
	//printf("rt_schedule:0x%08x\n",level);
	struct rt_thread *to_thread;
    struct rt_thread *from_thread=rt_current_thread;
	unsigned long highest_ready_priority;
	if (rt_thread_ready_priority_group != 0)
	{
		to_thread = _get_highest_priority_thread(&highest_ready_priority);
		//printf("from:%s,to:%s\n",from_thread->name,to_thread->name);
		//printf("from_thread:%s,tick:%d,to_thread:%s,tick:%d,current statu:0x%08X.\n",from_thread->name,
		//	from_thread->remaining_tick,to_thread->name,to_thread->remaining_tick,rt_current_thread->statu);
		
		if (to_thread != rt_current_thread)//找到了与当前线程不一样的任务 
		{
			rt_current_priority = (unsigned char)highest_ready_priority;
			rt_current_thread	= to_thread;
			
			to_thread->statu = RT_THREAD_RUNNING;
			if(from_thread->statu!=RT_THREAD_CLOSE&&from_thread->statu!=RT_THREAD_SLEEP&&from_thread->statu!=RT_THREAD_SUSPEND)//不在休眠状态,不是死亡线程 重新插入准备链表
				rt_schedule_insert_thread(from_thread);//将当前正在运行的线程重新插入准备线程中 以便下次可运行
			//printf("1:%d\n",counterS);
			rt_schedule_remove_thread(to_thread);//准备执行新线程 将其从可运行链表中移除
			//printf("2:%d\n",counterS);
			/*
			if(from_thread->statu==RT_THREAD_CLOSE)
			{
				printf("RT_THREAD_CLOSE threadName:%s\n",from_thread->name);
				rt_hw_context_switch_to((unsigned long)&to_thread->sp);//切换运行第一优先级程序 永不回头
				return;
			}
			else*/
			{
				//printf("3:%d\n",counterS);
				//printf("from %s sp:0x%08x,to %s sp:0x%08x,nest:%d\n",from_thread->name,from_thread->sp,to_thread->name,to_thread->sp,rt_interrupt_nest);
				_rt_scheduler_stack_check(to_thread);
				//printf("4:%d\n",counterS);
				/*
                printf("[%d]switch to priority#%d "
                         "thread:%.*s(sp:0x%08x), "
                         "from thread:%.*s(sp: 0x%08x)\n",
                         0, highest_ready_priority,
                         RT_NAME_MAX, to_thread->name, to_thread->sp,
                         RT_NAME_MAX, from_thread->name, from_thread->sp);
				*/
				if(rt_interrupt_nest==0)
					rt_hw_context_switch((unsigned long)&from_thread->sp,(unsigned long)&to_thread->sp);
				else
					rt_hw_context_switch_interrupt((unsigned long)&from_thread->sp,(unsigned long)&to_thread->sp);
				//printf("4:%d\n",counterS);
			}
		}
		else//在准备运行链表中找到的新线程 竟然还在运行 必须将其移除 否则别人永远没有机会运行了
		{
			//rt_schedule_remove_thread(rt_current_thread);
			//rt_current_thread->statu = RT_THREAD_RUNNING | (rt_current_thread->statu & ~RT_THREAD_STAT_MASK);
		}
	}
	rt_hw_interrupt_enable(level);
	
	//printf("rt_schedule:0x%08x  2\n",level);
}

void threadTask(void)
{
	register long level = rt_hw_interrupt_disable();
	scheduleCounterFlag++;
	//if(threadStartFlag==0)
	if(rt_current_thread==NULL)//thread系统调度未start
	{
		rt_hw_interrupt_enable(level);
		return;
	}
	
	rt_current_thread->remaining_tick--;
	if(rt_current_thread->remaining_tick==0)//当前运行线程tick用完了
	{	
		rt_current_thread->remaining_tick=rt_current_thread->init_tick;//重新赋值
		rt_schedule();//执行一次调度
	}
	
	rt_timer_check();
	
	rt_hw_interrupt_enable(level);
}

int thread_start(void)
{
	unsigned long priorityint;
	struct rt_thread *to_thread=_get_highest_priority_thread(&priorityint);
	printf("thread_start priorityint:%d,name:%s\n",priorityint,to_thread->name);
	rt_current_thread = to_thread;

	rt_schedule_remove_thread(to_thread);//从当前优先级的链表中移除
	to_thread->statu=RT_THREAD_RUNNING;
	
	//threadStartFlag=1;
	rt_hw_context_switch_to((unsigned long)&to_thread->sp);//切换运行第一优先级程序 永不回头

	return 0;
}

long rt_thread_suspend(rt_thread_t thread,char statu)//挂起线程
{
    register long stat;
    /* thread check */
    stat = thread->statu;
    if ((stat != RT_THREAD_READY) && (stat != RT_THREAD_RUNNING))
    {
		printf("%s thread suspend error,statu:0x%2x\n",thread->name,thread->statu);
		return -RT_ERROR;
    }
	//printf("%s thread suspend,statu:0x%2x\n",thread->name,thread->statu);

    /* change thread stat */
    rt_schedule_remove_thread(thread);//从ready list中移除
    thread->statu = statu;
	
	rt_timer_stop(&(thread->thread_timer));//停止定时器
	
    return RT_EOK;
}

long rt_thread_resume(rt_thread_t thread)//唤醒线程
{
    register long temp;

    /* thread check */
    RT_ASSERT(thread != RT_NULL);
    if (thread->statu  != RT_THREAD_SUSPEND&&thread->statu  != RT_THREAD_SLEEP)
    {
        //RT_DEBUG_LOG(RT_DEBUG_THREAD, ("thread resume: thread disorder, %d\n",
          //                             thread->stat));
        return -RT_ERROR;
    }

    /* disable interrupt */
    temp = rt_hw_interrupt_disable();

    /* remove from suspend list */
    rt_list_remove(&(thread->tlist));

    rt_timer_stop(&thread->thread_timer);

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);

    /* insert to schedule ready list */
    rt_schedule_insert_thread(thread);

    //RT_OBJECT_HOOK_CALL(rt_thread_resume_hook, (thread));
    return RT_EOK;
}

long rt_thread_sleep(unsigned int tick)
{
    register long temp= rt_hw_interrupt_disable();
    struct rt_thread *thread = rt_current_thread;//这个时候线程早就已经不再ready链表中了
    //printf("rt_thread_sleep,name:%s\n",thread->name);
    thread->thread_timer.init_tick=tick;
	
    /* suspend thread */
    rt_thread_suspend(thread,RT_THREAD_SLEEP);//这个时候线程已经不在ready链表中了
	
	rt_timer_start(&(thread->thread_timer));//这个操作会将定时器加入timer链表中
    /* enable interrupt */
    rt_hw_interrupt_enable(temp);
	
	rt_schedule();
    return 0;
}

long  rt_thread_delay(unsigned int tick)
{
    return rt_thread_sleep(tick);
}
static unsigned int threadIdIndex=0;
rt_thread_t thread_join_init(char* thread_name,void (*fun)(void *userdata),void *userdata,int stackSize,int priority,int runtick)
{
	register long level;
	if(rt_current_thread!=NULL)//说明此时线程已经运作
		level=rt_hw_interrupt_disable();


	if(priority>RT_THREAD_PRIORITY_MAX-1)
	{
		printf("!!!Creat thread priority ERROR,min:0,max:%d\n",RT_THREAD_PRIORITY_MAX-1);
		return NULL;
	}

	struct rt_thread *newThread=(struct rt_thread*)malloc(sizeof(struct rt_thread));
	if(newThread==NULL)
	{
		printf("!!!Creat thread mem ERROR.\n");
		return NULL;
	}
	printf("thread_join_init thread name:%s,newThread addr:0x%08X\n",thread_name,newThread);
	memset(newThread,0,sizeof(struct rt_thread));

	if(stackSize==0)
	{
		printf("ERROR,thread_join_init stackSize=0\n");
		free(newThread);
		return NULL;
	}
	newThread->stack_size=stackSize;
	newThread->stack_addr=malloc(newThread->stack_size);
	if(newThread->stack_addr==NULL)
	{
		printf("!!!Creat thread stackERROR.\n");
		free(newThread);
		return NULL;
	}
	printf("thread_join_init stackSize:%d,stack addr:0x%08x\n",stackSize,newThread->stack_addr);
	memset(newThread->stack_addr,'#',newThread->stack_size);//格式化成#

	int nameLen=strlen(thread_name);
	if(nameLen>RT_NAME_MAX-1)//裁剪限制名字长度
		nameLen=RT_NAME_MAX-1;
	memcpy(newThread->name,thread_name,nameLen);
	
	newThread->init_priority=priority;
	newThread->current_priority=priority;
	
	newThread->remaining_tick=runtick;
	newThread->init_tick=runtick;
	
	newThread->parameter=userdata;
	
	newThread->id=threadIdIndex;
	threadIdIndex++;

	rt_timer_init(&newThread->thread_timer,newThread->name,rt_thread_timeout,newThread,0,0);
    rt_list_init(&(newThread->tlist));
    rt_list_init(&(newThread->allList));
	rt_list_insert_after(&rt_all_thread,&(newThread->allList));
	
	newThread->number_mask = 1L << newThread->current_priority;//0~31
	
	newThread->sp=rt_hw_stack_init(fun,userdata,newThread->stack_addr+newThread->stack_size-sizeof(unsigned long),rt_thread_exit);
    newThread->statu = RT_THREAD_READY;
	
	rt_thread_ready_priority_group |= newThread->number_mask;
    rt_list_insert_before(&(rt_thread_priority_table[newThread->current_priority]),&(newThread->tlist));
	
	if(rt_current_thread!=NULL)//说明此时线程已经运作
		rt_hw_interrupt_enable(level);
	
	return newThread;
}

rt_thread_t find_thread_by_id(unsigned int id)
{
	struct rt_thread *thread;
	rt_list_for_each_entry(thread,&rt_all_thread,allList)
	{
		if(thread!=NULL&&thread->id==id)
			return thread;
	}
	return NULL;
}


void idle_thread(void*userdata)//空闲线程 做一些死掉的线程清理工作
{
	//int count=10000;
	while(1)
	{
		register long temp= rt_hw_interrupt_disable();
		//printf("idle_thread:%d\n",count);
		//count++;
		struct rt_thread *dead_thread,*posTmp;
		if (!rt_list_isempty(&rt_thread_defunct))
		rt_list_for_each_entry_safe(dead_thread,posTmp,&rt_thread_defunct,tlist)
		{
			printf("idle_thread clean closed thread, name:%s\n",dead_thread->name);
			///*
			rt_list_remove(&(dead_thread->tlist));
			rt_list_remove(&(dead_thread->allList));
			if(dead_thread->statu==RT_THREAD_CLOSE)//死亡线程
			{
				free(dead_thread->stack_addr);
				free(dead_thread);
			}
			//*/
		}
		rt_hw_interrupt_enable(temp);
		//rt_thread_delay(1000);//不要增加延时，否则系统将无线程可以调度了
	}
}

int thread_init(void)
{

	int offset=0;
    for (offset = 0; offset < RT_THREAD_PRIORITY_MAX; offset ++)
    {
        rt_list_init(&rt_thread_priority_table[offset]);
    }
	rt_list_init(&rt_thread_defunct);

	rt_list_init(&rt_all_thread);
	
	rt_list_init(&rt_timer_list);

	rt_thread_ready_priority_group=0;
	
	thread_join_init("idle_thread",idle_thread,NULL,4096,RT_THREAD_PRIORITY_MAX-1,20);//至少加入一个空闲线程，系统管理用 给它倒数第1的优先级
	return 0;
}



//=============================sem=============================
long rt_sem_init(rt_sem_t    sem,const char *name,unsigned int value,unsigned char  flag)
{
    //RT_ASSERT(sem != RT_NULL);
    //RT_ASSERT(value < 0x10000U);

    /* init object */
    //rt_object_init(&(sem->parent.parent), RT_Object_Class_Semaphore, name);

    /* init ipc object */
    //rt_ipc_object_init(&(sem->parent));
    rt_list_init(&(sem->suspend_thread_list));

    /* set init value */
    sem->value = (unsigned short)value;

    /* set parent */
    sem->flag = flag;


    return RT_EOK;
}

rt_sem_t rt_sem_create(const char *name, unsigned int value, unsigned char flag)
{
    rt_sem_t sem;

    //RT_DEBUG_NOT_IN_INTERRUPT;
    RT_ASSERT(value < 0x10000U);

    /* allocate object */
    sem = (rt_sem_t)malloc(sizeof(struct rt_semaphore));
	memset(sem,0,sizeof(struct rt_semaphore));
    if (sem == RT_NULL)
        return sem;

    /* init ipc object */
    //rt_ipc_object_init(&(sem->parent));
	rt_list_init(&(sem->suspend_thread_list));

    /* set init value */
    sem->value = value;

    /* set parent */
    sem->flag = flag;

    return sem;
}

long rt_sem_delete(rt_sem_t sem)
{
   // RT_DEBUG_NOT_IN_INTERRUPT;

    /* parameter check */
    RT_ASSERT(sem != RT_NULL);
    //RT_ASSERT(rt_object_get_type(&sem->parent.parent) == RT_Object_Class_Semaphore);
    //RT_ASSERT(rt_object_is_systemobject(&sem->parent.parent) == RT_FALSE);

    /* wakeup all suspend threads */
    //rt_ipc_list_resume_all(&(sem->parent.suspend_thread));
	struct rt_thread *threadP, *posTmp;
	rt_list_for_each_entry_safe(threadP,posTmp,&sem->suspend_thread_list,tlist)
	{
		rt_thread_resume(threadP);
	}
    /* delete semaphore object */
    //rt_object_delete(&(sem->parent.parent));
    free(sem);

    return RT_EOK;
}


#ifdef PS_DEBUG_SEM
int ps_sem_suspend_thread(char *flag,rt_sem_t sem)
{
	printf("\n=========================%s,sem value:%d===========================\n",flag,sem->value);
	if(rt_current_thread!=NULL)
	{
		printf("running thread name:%s,remainTick:%d,suspend thread:\n",rt_current_thread->name,rt_current_thread->remaining_tick);
		struct rt_thread *p;
		
	    if (!rt_list_isempty(&sem->suspend_thread_list))//该信号量中含有被挂起的线程
		rt_list_for_each_entry(p,&sem->suspend_thread_list,tlist)
		{
			printf("%s\n",p->name);
		}
	}
	printf("=============================end====================================\n\n");
	return 0;
}
#else
	#define ps_sem_suspend_thread(flag,sem) 
#endif

long rt_sem_take(rt_sem_t sem, int time)
{
    register long temp= rt_hw_interrupt_disable();

	ps_sem_suspend_thread("rt_sem_take1",sem);
    if (sem->value > 0)//获取成功
    {
        /* semaphore is available */
        sem->value --;

        /* enable interrupt */
        rt_hw_interrupt_enable(temp);
    }
    else//获取失败 
    {
        /* no waiting, return with timeout */
        if (time == 0)//不应该这样
        {
            rt_hw_interrupt_enable(temp);

            return -RT_ETIMEOUT;
        }
        else//挂起操作
        {
			struct rt_thread *thread = rt_thread_self();
			if(thread==NULL)
			{
				printf("thread not start!!!\n");
				rt_hw_interrupt_enable(temp);
				return -RT_ERROR;
			}
            /* reset thread error number */
            thread->error = RT_EOK;

            /* suspend thread */
			rt_thread_suspend(thread,RT_THREAD_SUSPEND);
			rt_list_insert_before(&(sem->suspend_thread_list), &(thread->tlist));//将线程挂起至信号量链表中
			
            if (time > 0)//设置了超时时间
            {
                thread->thread_timer.init_tick=time;
				rt_timer_start(&thread->thread_timer);
            }
			ps_sem_suspend_thread("rt_sem_take2",sem);
			
            /* enable interrupt */
            rt_hw_interrupt_enable(temp);

            /* do schedule */
            rt_schedule();

            if (thread->error != RT_EOK)
            {
                return thread->error;
            }
        }
    }
    return RT_EOK;
}

long rt_sem_release(rt_sem_t sem)
{
    register long temp=rt_hw_interrupt_disable();
    register char need_schedule=0;
	
	ps_sem_suspend_thread("rt_sem_release",sem);
	
    if (!rt_list_isempty(&sem->suspend_thread_list))//该信号量中含有被挂起的线程
    {
		/* resume the suspended thread */
        //rt_ipc_list_resume(&(sem->parent.suspend_thread));
		struct rt_thread *thread;
		/* get thread entry */
		thread = rt_list_entry(sem->suspend_thread_list.next, struct rt_thread, tlist);
		/* resume it */
		rt_list_remove(&(thread->tlist));//从该信号量被挂起的线程链表中唤起一个线程
		
		rt_timer_stop(&thread->thread_timer);
		
		/* insert to schedule ready list */
		rt_schedule_insert_thread(thread);
			
        need_schedule = 1;
    }
    else//没有被挂起的线程 +1
        sem->value ++; /* increase value */

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);

    /* resume a thread, re-schedule */
    if (need_schedule == 1)
        rt_schedule();

    return RT_EOK;
}

long rt_sem_detach(rt_sem_t sem)
{
    /* parameter check */
    RT_ASSERT(sem != RT_NULL);
    //RT_ASSERT(rt_object_get_type(&sem->parent.parent) == RT_Object_Class_Semaphore);
    //RT_ASSERT(rt_object_is_systemobject(&sem->parent.parent));

    /* wakeup all suspend threads */
    //rt_ipc_list_resume_all(&(sem->parent.suspend_thread));
    register long temp= rt_hw_interrupt_disable();
	
	struct rt_thread *threadP, *posTmp;
	rt_list_for_each_entry_safe(threadP,posTmp,&sem->suspend_thread_list,tlist)
	{
		rt_thread_resume(threadP);
	}

    /* detach semaphore object */
    //rt_object_detach(&(sem->parent.parent));
    rt_hw_interrupt_enable(temp);
	
    return RT_EOK;
}



long rt_mb_init(rt_mailbox_t mb,const char  *name,void        *msgpool,unsigned long    size,unsigned char   flag)
{
    RT_ASSERT(mb != RT_NULL);

    /* init object */
    //rt_object_init(&(mb->parent.parent), RT_Object_Class_MailBox, name);

    /* set parent flag */
    mb->flag = flag;

    /* init ipc object */
    //rt_ipc_object_init(&(mb->parent));
	rt_list_init(&(mb->suspend_thread_list));

    /* init mailbox */
    mb->msg_pool   = (unsigned long *)msgpool;
    mb->size       = size;
    mb->entry      = 0;
    mb->in_offset  = 0;
    mb->out_offset = 0;

    /* init an additional list of sender suspend thread */
    rt_list_init(&(mb->suspend_sender_thread));

    return RT_EOK;
}

rt_mailbox_t rt_mb_create(const char *name, unsigned int size, unsigned char flag)
{
    rt_mailbox_t mb;

    //RT_DEBUG_NOT_IN_INTERRUPT;

    /* allocate object */
    mb = (rt_mailbox_t)malloc(sizeof(struct rt_mailbox));
    if (mb == RT_NULL)
        return mb;

    /* set parent */
    mb->flag = flag;

    /* init ipc object */
    //rt_ipc_object_init(&(mb->parent));
	rt_list_init(&(mb->suspend_thread_list));

    /* init mailbox */
    mb->size     = size;
    mb->msg_pool = (unsigned long *)malloc(mb->size*sizeof(unsigned long));
    if (mb->msg_pool == RT_NULL)
    {
        /* delete mailbox object */
        //rt_object_delete(&(mb->parent.parent));
        return RT_NULL;
    }
    mb->entry      = 0;
    mb->in_offset  = 0;
    mb->out_offset = 0;

    /* init an additional list of sender suspend thread */
    rt_list_init(&(mb->suspend_sender_thread));

    return mb;
}

long rt_mb_delete(rt_mailbox_t mb)
{
    //RT_DEBUG_NOT_IN_INTERRUPT;

    /* parameter check */
    RT_ASSERT(mb != RT_NULL);

    /* resume all suspended thread */
    //rt_ipc_list_resume_all(&(mb->parent.suspend_thread));
	struct rt_thread *threadP, *posTmp;
	rt_list_for_each_entry_safe(threadP,posTmp,&mb->suspend_thread_list,tlist)
	{
		rt_thread_resume(threadP);
	}

    /* also resume all mailbox private suspended thread */
    //rt_ipc_list_resume_all(&(mb->suspend_sender_thread));
	rt_list_for_each_entry_safe(threadP,posTmp,&mb->suspend_sender_thread,tlist)
	{
		rt_thread_resume(threadP);
	}

    /* free mailbox pool */
    free(mb->msg_pool);

    /* delete mailbox object */
   // rt_object_delete(&(mb->parent.parent));

    return RT_EOK;
}

long rt_mb_send_wait(rt_mailbox_t mb,unsigned long value,int timeout)
{
    struct rt_thread *thread;
    register long temp;
    unsigned int tick_delta;

    /* parameter check */
    RT_ASSERT(mb != RT_NULL);
    //RT_ASSERT(rt_object_get_type(&mb->parent.parent) == RT_Object_Class_MailBox);

    /* initialize delta tick */
    tick_delta = 0;
    /* get current thread */
    thread = rt_thread_self();
	if(thread==NULL)
	{
		printf("thread not start!!!\n");
		return -RT_ERROR;
	}

    //RT_OBJECT_HOOK_CALL(rt_object_put_hook, (&(mb->parent.parent)));

    /* disable interrupt */
    temp = rt_hw_interrupt_disable();

    /* for non-blocking call */
    if (mb->entry == mb->size && timeout == 0)
    {
        rt_hw_interrupt_enable(temp);

        return -RT_EFULL;
    }

    /* mailbox is full */
    while (mb->entry == mb->size)
    {
        /* reset error number in thread */
        thread->error = RT_EOK;

        /* no waiting, return timeout */
        if (timeout == 0)
        {
            /* enable interrupt */
            rt_hw_interrupt_enable(temp);

            return -RT_EFULL;
        }

        //RT_DEBUG_IN_THREAD_CONTEXT;
        /* suspend current thread */
        //rt_ipc_list_suspend(&(mb->suspend_sender_thread),
        //                    thread,
        //                    mb->parent.parent.flag);
        
		rt_thread_suspend(thread,RT_THREAD_SUSPEND);
        rt_list_insert_before(&mb->suspend_sender_thread, &(thread->tlist));

        /* has waiting time, start thread timer */
        if (timeout > 0)
        {
            /* get the start tick of timer */
            tick_delta = sys_tick_get();

            //RT_DEBUG_LOG(RT_DEBUG_IPC, ("mb_send_wait: start timer of thread:%s\n",
            //                            thread->name));

            /* reset the timeout of thread timer and start it */
            /*
            rt_timer_control(&(thread->thread_timer),
                             RT_TIMER_CTRL_SET_TIME,
                             &timeout);
            rt_timer_start(&(thread->thread_timer));
			*/
			
			thread->thread_timer.init_tick=timeout;
			rt_timer_start(&thread->thread_timer);
        }

        /* enable interrupt */
        rt_hw_interrupt_enable(temp);

        /* re-schedule */
        rt_schedule();

        /* resume from suspend state */
        if (thread->error != RT_EOK)
        {
            /* return error */
            return thread->error;
        }

        /* disable interrupt */
        temp = rt_hw_interrupt_disable();

        /* if it's not waiting forever and then re-calculate timeout tick */
        if (timeout > 0)
        {
            tick_delta = sys_tick_get() - tick_delta;
            timeout -= tick_delta;
            if (timeout < 0)
                timeout = 0;
        }
    }

    /* set ptr */
    mb->msg_pool[mb->in_offset] = value;
    /* increase input offset */
    ++ mb->in_offset;
    if (mb->in_offset >= mb->size)
        mb->in_offset = 0;
    /* increase message entry */
    mb->entry ++;

    /* resume suspended thread */
    if (!rt_list_isempty(&mb->suspend_thread_list))
    {
        //rt_ipc_list_resume(&(mb->parent.suspend_thread));
	    thread = rt_list_entry(mb->suspend_thread_list.next, struct rt_thread, tlist);

	    //RT_DEBUG_LOG(RT_DEBUG_IPC, ("resume thread:%s\n", thread->name));
	    /* resume it */
	    rt_thread_resume(thread);

        /* enable interrupt */
        rt_hw_interrupt_enable(temp);

        rt_schedule();

        return RT_EOK;
    }

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);

    return RT_EOK;
}

long rt_mb_send(rt_mailbox_t mb, unsigned long value)
{
    return rt_mb_send_wait(mb, value, 0);
}

long rt_mb_recv(rt_mailbox_t mb, unsigned long *value, unsigned int timeout)
{
    struct rt_thread *thread;
    register long temp;
    unsigned int tick_delta;

    /* parameter check */
    RT_ASSERT(mb != RT_NULL);
    //RT_ASSERT(rt_object_get_type(&mb->parent.parent) == RT_Object_Class_MailBox);

    /* initialize delta tick */
    tick_delta = 0;
    /* get current thread */
    thread = rt_thread_self();
	if(thread==NULL)
	{
		printf("thread not start!!!\n");
		return -RT_ERROR;
	}

    //RT_OBJECT_HOOK_CALL(rt_object_trytake_hook, (&(mb->parent.parent)));

    /* disable interrupt */
    temp = rt_hw_interrupt_disable();

    /* for non-blocking call */
    if (mb->entry == 0 && timeout == 0)
    {
        rt_hw_interrupt_enable(temp);

        return -RT_ETIMEOUT;
    }

    /* mailbox is empty */
    while (mb->entry == 0)
    {
        /* reset error number in thread */
        thread->error = RT_EOK;

        /* no waiting, return timeout */
        if (timeout == 0)
        {
            /* enable interrupt */
            rt_hw_interrupt_enable(temp);

            thread->error = -RT_ETIMEOUT;

            return -RT_ETIMEOUT;
        }

        //RT_DEBUG_IN_THREAD_CONTEXT;
        /* suspend current thread */
        //rt_ipc_list_suspend(&(mb->parent.suspend_thread),
        //                    thread,
         //                   mb->parent.parent.flag);
		
		rt_thread_suspend(thread,RT_THREAD_SUSPEND);
        rt_list_insert_before(&mb->suspend_thread_list, &(thread->tlist));

        /* has waiting time, start thread timer */
        if (timeout > 0)
        {
            /* get the start tick of timer */
            tick_delta = sys_tick_get();

            //RT_DEBUG_LOG(RT_DEBUG_IPC, ("mb_recv: start timer of thread:%s\n",
            //                            thread->name));

            /* reset the timeout of thread timer and start it */
            /*rt_timer_control(&(thread->thread_timer),
                             RT_TIMER_CTRL_SET_TIME,
                             &timeout);
			
            rt_timer_start(&(thread->thread_timer));
            */
			thread->thread_timer.init_tick=timeout;
			rt_timer_start(&thread->thread_timer);
        }

        /* enable interrupt */
        rt_hw_interrupt_enable(temp);

        /* re-schedule */
        rt_schedule();

        /* resume from suspend state */
        if (thread->error != RT_EOK)
        {
            /* return error */
            return thread->error;
        }

        /* disable interrupt */
        temp = rt_hw_interrupt_disable();

        /* if it's not waiting forever and then re-calculate timeout tick */
        if (timeout > 0)
        {
            tick_delta = sys_tick_get() - tick_delta;
            timeout -= tick_delta;
            if (timeout < 0)
                timeout = 0;
        }
    }

    /* fill ptr */
    *value = mb->msg_pool[mb->out_offset];

    /* increase output offset */
    ++ mb->out_offset;
    if (mb->out_offset >= mb->size)
        mb->out_offset = 0;
    /* decrease message entry */
    mb->entry --;

    /* resume suspended thread */
    if (!rt_list_isempty(&(mb->suspend_sender_thread)))
    {
        //rt_ipc_list_resume(&(mb->suspend_sender_thread));
		struct rt_thread *thread;
		/* get thread entry */
		thread = rt_list_entry(mb->suspend_sender_thread.next, struct rt_thread, tlist);
		/* resume it */
		rt_list_remove(&(thread->tlist));//从该信号量被挂起的线程链表中唤起一个线程
		
		rt_timer_stop(&thread->thread_timer);
		
		/* insert to schedule ready list */
		rt_schedule_insert_thread(thread);
		
        /* enable interrupt */
        rt_hw_interrupt_enable(temp);
        //RT_OBJECT_HOOK_CALL(rt_object_take_hook, (&(mb->parent.parent)));
        rt_schedule();

        return RT_EOK;
    }

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);

    //RT_OBJECT_HOOK_CALL(rt_object_take_hook, (&(mb->parent.parent)));

    return RT_EOK;
}




//=================================cmd===========================================
#include "shell.h"

#define LIST_FIND_OBJ_NR 8
typedef struct
{
    rt_list_t *list;
    rt_list_t **array;
    unsigned char type;
    int nr;             /* input: max nr, can't be 0 */
    int nr_out;         /* out: got nr */
} list_get_next_t;

long list_thread(void)
{
	long level = rt_hw_interrupt_disable();

	const char item_title[] = "thread";
	unsigned char idChar[8]="";
	unsigned char statuChar[8]="";
	unsigned char spAddrChar[12]="";
	unsigned char stackSizeChar[12]="";
	unsigned char stackUsedChar[10]="";
	unsigned char remainTickChar[12]="";
	unsigned char errorChar[8]="";
	
	char stat=-1;
	unsigned char *ptr=NULL;
	int maxlen= RT_NAME_MAX;
	
	//printf("%-*.s pri  status	   sp	  stack_size max_used left_tick  error\n", maxlen, item_title);
	printf("%-*.*s ", 4, 4, "ID");//点前面*号代表字段宽度，后面的*表示保留的位数
	printf("%-*.*s ", maxlen, RT_NAME_MAX, item_title);//点前面*号代表字段宽度，后面的*表示保留的位数
	printf("%-*.*s ", 3, 3, "pri");//点前面*号代表字段宽度，后面的*表示保留的位数
	printf("%-*.*s ",8,8,"status");
	printf("%-*.*s ",10,10,"sp");
	printf("%-*.*s ",10,10,"stack_size");
	printf("%-*.*s ",8,8,"max_used");
	printf("%-*.*s ",10,10,"left_tick");
	printf("%-*.*s \n",5,5,"error");

	printf("---- ------------------------ --- -------- ---------- ---------- -------- ---------- -----\n");

	struct rt_thread *thread;
	rt_list_for_each_entry(thread,&rt_all_thread,allList)
	{
		sprintf(idChar,"%04d",thread->id);
		printf("%-*.*s ", 4, 4, idChar);//点前面*号代表字段宽度，后面的*表示保留的位数
		
		printf("%-*.*s %3d ", maxlen, RT_NAME_MAX, thread->name, thread->current_priority);//点前面*号代表字段宽度，后面的*表示保留的位数
		stat = thread->statu;
		
		if (stat == RT_THREAD_READY)		sprintf(statuChar,"%s","ready");
		else if (stat == RT_THREAD_SUSPEND) sprintf(statuChar,"%s","suspend");
		else if (stat == RT_THREAD_INIT)	sprintf(statuChar,"%s","init");
		else if (stat == RT_THREAD_CLOSE)	sprintf(statuChar,"%s","close");
		else if (stat == RT_THREAD_RUNNING) sprintf(statuChar,"%s","running");
		else if (stat == RT_THREAD_SLEEP) sprintf(statuChar,"%s","sleep");
		printf("%-*.*s ",8,8,statuChar);
		
		ptr = (unsigned char *)thread->stack_addr;
		while (*ptr == '#')ptr ++;

		sprintf(spAddrChar,"0x%08x",thread->stack_size + ((unsigned long)thread->stack_addr - (unsigned long)thread->sp));//sp位置
		printf("%-*.*s ",10,10,spAddrChar);
		
		sprintf(stackSizeChar,"%08d",thread->stack_size);
		printf("%-*.*s ",10,10,stackSizeChar);
		
		sprintf(stackUsedChar,"%02d%%",(thread->stack_size - ((unsigned long) ptr - (unsigned long) thread->stack_addr)) * 100/ thread->stack_size);
		printf("%-*.*s ",8,8,stackUsedChar);
		
		sprintf(remainTickChar,"0x%08x",thread->remaining_tick);
		printf("%-*.*s ",10,10,remainTickChar);
		
		sprintf(errorChar,"%03d",thread->error);
		printf("%-*.*s \n",5,5,errorChar);

		
		/*
		printf(" 0x%08x %08d	  %02d%%   0x%08x %03d\n",
				thread->stack_size + ((unsigned long)thread->stack_addr - (unsigned long)thread->sp),//sp位置
				thread->stack_size,
				(thread->stack_size - ((unsigned long) ptr - (unsigned long) thread->stack_addr)) * 100/ thread->stack_size,//堆使用空间百分比
				thread->remaining_tick,
				thread->error);
		*/
	}
    rt_hw_interrupt_enable(level);
	return 0;
}
FINSH_FUNCTION_EXPORT(list_thread,print all thread in sys);

int ps_thread(void)
{
	int i=0;
	struct rt_thread *p;
	for(i=0;i<RT_THREAD_PRIORITY_MAX;i++)
	rt_list_for_each_entry(p,&rt_thread_priority_table[i],tlist)
	{
		printf("priority:%d,thread name:%s\n",i,p->name);
	}
}




