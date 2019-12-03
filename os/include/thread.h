#ifndef _THREAD_H_
#define _THREAD_H_

typedef int32_t		       register_t;
typedef int 		 f_register_t;
struct threadframe {
	register_t	zero;
	register_t	at;
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
	register_t	s7;//24
	
	register_t	t8;//25
	register_t	t9;
	
	register_t	k0;
	register_t	k1;
	
	register_t	gp;
	register_t	sp;//stack pointer CPU 按照 SP 指针，到存储器存取地址或数据。
	
	register_t	s8;
	register_t	ra;//32
	
	register_t	sr;//33statu register 
	register_t	mullo;//34
	register_t	mulhi;//35
	register_t	badvaddr;//36
	register_t	cause;//37
	register_t	pc;//38 Program Counter CPU 按照 PC 指针，到存储器去取指令代码。

	/*
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
	*/
};

struct rt_thread{
	char 			name[RT_NAME_MAX];
    unsigned char  	type;                                   /**< type of object */
    unsigned char  	flags;                                  /**< thread's flags */
    rt_list_t   	list;                                   /**< the object list */
	
    rt_list_t   	tlist;                                  /**< the thread list */
	rt_list_t		allList;								//用于存储所有的链表

	unsigned int	id;										//thread id dstling add
	
	unsigned char current_priority;
	unsigned char init_priority;
	
	unsigned int init_tick;
	unsigned int remaining_tick;

	unsigned int number_mask;

	void 	*parameter;
	
	char 	statu;// 0可运行RUN_ABLE_THREAD    1挂起暂停休眠SLEEP_THREAD 2线程任务结束DEAD_THREAD 可以清理删除

    void       *sp;                                     /**< stack point */
    void       *entry;                                  /**< entry */
    void       *stack_addr;                             /**< stack address */
    unsigned int stack_size;                             /**< stack size */

    long    error;                                  /**< error code */

	struct rt_timer thread_timer;
	struct threadframe *tf;
};

typedef struct rt_thread *rt_thread_t;
extern struct rt_thread *rt_current_thread;

#endif

