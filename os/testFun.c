#include <buildconfig.h>
#include <types.h>
#include <stdio.h>
#include <ls1c_regs.h>
#include <ls1c_mipsregs.h>
#include "shell.h"
#include "start.h"
#include <rtdef.h>

#define readl(addr) (*(volatile unsigned long *)(addr))
#define writel(val, addr) (*(volatile unsigned long *)(addr) = (val))
//#define read_c0_config1()		__read_32bit_c0_register(COP_0_PRID, 1)

void readCpuId(void)
{
	unsigned long cpuid=read_c0_prid();
	printf("cpu type id:0x%08x\n",cpuid);
}
FINSH_FUNCTION_EXPORT(readCpuId,print readCpuId);
extern jmp_buf 		jmpbMainStart;		/* non-local goto jump buffer */
extern long flagJmpMain;

void returnMain(void)
{
	long level=rt_hw_interrupt_disable();
	sys_tick_disable();
	irq_disable_all();
	rt_current_thread=NULL;//无论如何要停止线程调度
	flagJmpMain=1;
	longjmp(jmpbMainStart);
	//never herer;
}
FINSH_FUNCTION_EXPORT(returnMain,returnMain);


void printf_pllreg(void)
{
	unsigned int pllreg0value=readl(LS1C_START_FREQ);
	unsigned int pllreg1value=readl(LS1C_CLK_DIV_PARAM);
	unsigned int pll_rate=clk_get_pll_rate();
	printf("printf_pllreg pllreg0value:0x%08x\n",pllreg0value);
	printf("printf_pllreg pllreg1value:0x%08x\n",pllreg1value);
	printf("printf_pllreg pll_rate:0x%08x,%d\n",pll_rate,pll_rate);
	clk_print_all();
}
FINSH_FUNCTION_EXPORT(printf_pllreg,print printf_pllreg);

void set_pll_add1(void)
{
    unsigned int ctrl;
    unsigned long pll_rate = 0;
	unsigned long pll_zhengshu=0;
	unsigned long pll_xiaoshu=0;
    ctrl = reg_read_32((volatile unsigned int *)LS1C_START_FREQ);
	pll_zhengshu=((ctrl & M_PLL) >> M_PLL_SHIFT);
	pll_xiaoshu=((ctrl & FRAC_N) >> FRAC_N_SHIFT);
    pll_rate= (pll_zhengshu + pll_xiaoshu) * APB_CLK / 4;
	

	printf("printf_pllreg ctrl:0x%08x\n",ctrl);
	printf("set_pll_add1 pll_zhengshu:%d\n",pll_zhengshu);
	printf("set_pll_add1 pll_xiaoshu:%d\n",pll_xiaoshu);
	printf("set_pll_add1 pll_rate:0x%08x,%d\n",pll_rate,pll_rate);

	pll_zhengshu+=1;
	pll_zhengshu<<=M_PLL_SHIFT;
	ctrl=(ctrl&(~M_PLL)|pll_zhengshu);
	printf("printf_pllreg new ctrl:0x%08x\n",ctrl);
	
	unsigned int pllreg1value=readl(LS1C_CLK_DIV_PARAM);
	printf("printf_pllreg1  pllreg1value:0x%08x\n",pllreg1value);
	pllreg1value=pllreg1value&(~DIV_CPU_SEL) ;//DIV_CPU_VALID
	printf("printf_pllreg2  pllreg1value:0x%08x\n",pllreg1value);
	writel(pllreg1value,LS1C_CLK_DIV_PARAM);

	pllreg1value=readl(LS1C_CLK_DIV_PARAM);
	printf("printf_pllreg3  pllreg1value:0x%08x\n",pllreg1value);
	
	writel(ctrl,LS1C_START_FREQ);//写入频率信息


	pllreg1value=readl(LS1C_CLK_DIV_PARAM);
	pllreg1value=pllreg1value|DIV_CPU_SEL ;//DIV_CPU_VALID
	printf("printf_pllreg4  pllreg1value:0x%08x\n",pllreg1value);
	writel(pllreg1value,LS1C_CLK_DIV_PARAM);
	
    ctrl = reg_read_32((volatile unsigned int *)LS1C_START_FREQ);
	printf("printf_pllreg writed ctrl:0x%08x,pllreg1value:0x%08x\n",ctrl,pllreg1value);
	clk_print_all();
}
FINSH_FUNCTION_EXPORT(set_pll_add1,print set_pll_add1);

void set_pll_add2(void)
{
    unsigned int ctrl;
    unsigned long pll_rate = 0;
	unsigned long pll_zhengshu=0;
	unsigned long pll_xiaoshu=0;
    ctrl = reg_read_32((volatile unsigned int *)LS1C_START_FREQ);
	pll_zhengshu=((ctrl & M_PLL) >> M_PLL_SHIFT);
	pll_xiaoshu=((ctrl & FRAC_N) >> FRAC_N_SHIFT);
    pll_rate= (pll_zhengshu + pll_xiaoshu) * APB_CLK / 4;
	
	printf("printf_pllreg ctrl:0x%08x\n",ctrl);
	printf("set_pll_add1 pll_zhengshu:%d\n",pll_zhengshu);
	printf("set_pll_add1 pll_xiaoshu:%d\n",pll_xiaoshu);
	printf("set_pll_add1 pll_rate:0x%08x,%d\n",pll_rate,pll_rate);

	pll_xiaoshu+=1;
	pll_xiaoshu<<=FRAC_N_SHIFT;
	ctrl=(ctrl&(~FRAC_N)|pll_xiaoshu);
	printf("printf_pllreg new ctrl:0x%08x\n",ctrl);
	writel(ctrl,LS1C_START_FREQ);
    ctrl = reg_read_32((volatile unsigned int *)LS1C_START_FREQ);
	printf("printf_pllreg writed ctrl:0x%08x\n",ctrl);
	clk_print_all();
}
FINSH_FUNCTION_EXPORT(set_pll_add2,print set_pll_add2);


void set_cpu_div_add1(void)
{
    unsigned long pll_rate, cpu_rate;
    unsigned int ctrl;
	unsigned int cpu_div=0;

    pll_rate = clk_get_pll_rate();
    ctrl = reg_read_32((volatile unsigned int *)LS1C_CLK_DIV_PARAM);
    if (DIV_CPU_SEL & ctrl)     // pll分频作为时钟信号
    {
        if (DIV_CPU_EN & ctrl)
        {
			cpu_div=(ctrl & DIV_CPU) >> DIV_CPU_SHIFT;
			cpu_rate = pll_rate / cpu_div;
			printf("pll_rate:%d/cpu_div:%d=cpu_rate:%d\n",pll_rate,cpu_div,cpu_rate);
			
        }
        else
        {
            cpu_rate = pll_rate / 2;
        }
    }
}
FINSH_FUNCTION_EXPORT(set_cpu_div_add1,print set_cpu_div_add1);

void set_clk_div_par(void)
{
    unsigned long pll_rate, cpu_rate;
    unsigned int ctrl;
	unsigned int cpu_div=0;

    pll_rate = clk_get_pll_rate();
    ctrl = reg_read_32((volatile unsigned int *)LS1C_CLK_DIV_PARAM);
	printf("set_clk_div_par ctrl :0x%08x\n",ctrl);
	writel(0x00008283,LS1C_CLK_DIV_PARAM);
	
    ctrl = reg_read_32((volatile unsigned int *)LS1C_CLK_DIV_PARAM);
	printf("set_clk_div_par ctrl2 :0x%08x\n",ctrl);
	
}
FINSH_FUNCTION_EXPORT(set_clk_div_par,print set_clk_div_par);


void printf_cpu_count(void)
{
	printf("cpu_count:%d\n",CPU_GetCOUNT());
}
FINSH_FUNCTION_EXPORT(printf_cpu_count,print printf_cpu_count);
void printf_c0_status(void)
{
	//int ret=read_c0_status();
	//printf("read_c0_status:0x%08x\n",ret);
	printf("printf_c0_status happens,epc: 0x%08x\n", 	read_c0_epc());
	printf("printf_c0_status,CAUSE: 0x%08x\n", 			read_c0_cause());
	printf("printf_c0_status,STATU: 0x%08x\n", 			read_c0_status());
	printf("printf_c0_status,BAD_VADDR: 0x%08x\n", 		read_c0_badvaddr());
}
FINSH_FUNCTION_EXPORT(printf_c0_status,printf_c0_status);

typedef	long	time_t;
#define	inl(a)		(*(volatile unsigned int*)(a))
#define REG_TOY_READ0 0xbfe6402c//RTC TOYTOY 低 32 位读出时间
#define REG_TOY_READ1 0xbfe64030//RTC TOYTOY 高 32 位读出年
#define REG_TOY_WRITE0 0xbfe64024//写时间
#define REG_TOY_WRITE1 0xbfe64028//写年
struct tm {
	int	tm_sec100ms;		//0.1s
	int	tm_sec;		/* seconds after the minute [0-60] */
	int	tm_min;		/* minutes after the hour [0-59] */
	int	tm_hour;	/* hours since midnight [0-23] */
	int	tm_mday;	/* day of the month [1-31] */
	int	tm_mon;		/* months since January [0-11] */
	int	tm_year;	/* years since 1900 */
	int	tm_wday;	/* days since Sunday [0-6] */
	int	tm_yday;	/* days since January 1 [0-365] */
	int	tm_isdst;	/* Daylight Savings Time flag */
	long	tm_gmtoff;	/* offset from CUT in seconds */
	char	*tm_zone;	/* timezone abbreviation */
};
#define	SECS_PER_MIN	60
#define	MINS_PER_HOUR	60
#define	HOURS_PER_DAY	24
#define	DAYS_PER_WEEK	7
#define	DAYS_PER_NYEAR	365
#define	DAYS_PER_LYEAR	366
#define	SECS_PER_HOUR	(SECS_PER_MIN * MINS_PER_HOUR)
#define	SECS_PER_DAY	((long) SECS_PER_HOUR * HOURS_PER_DAY)
#define	MONS_PER_YEAR	12
static const int	mon_lengths[2][MONS_PER_YEAR] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};
static const int	year_lengths[2] = {
	DAYS_PER_NYEAR, DAYS_PER_LYEAR
};
#define	isleap(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))
#define FROM_YEAR 2000
#define TO_YEAR 2100
time_t  gmmktime(const struct tm *tm)//计算从FROM_YEAR年至现在的秒数
{
    int yr;
    int mn;
    time_t secs;

    if (tm->tm_sec > 59 ||tm->tm_min > 59 ||tm->tm_hour > 23 ||tm->tm_mday > 31 ||tm->tm_mon > 12 ||tm->tm_year < FROM_YEAR)
    {
		printf("time date's datas format error.\n");
        return (time_t)-1;
    }

    secs = tm->tm_sec;
    secs += tm->tm_min * SECS_PER_MIN;
    secs += tm->tm_hour * SECS_PER_HOUR;
    secs += (tm->tm_mday-1) * SECS_PER_DAY;

    for (mn = 0; mn < tm->tm_mon; mn++)
    	secs += mon_lengths[isleap(tm->tm_year-1900)][mn] * SECS_PER_DAY;
    for(yr=FROM_YEAR; yr < tm->tm_year; yr++)
      secs += year_lengths[isleap(yr)]*SECS_PER_DAY;
      
    return secs;
}

int date(void)
{
	struct tm tms;
	time_t totalSecs=0;
	unsigned int year=0,v=0;
	//unsigned int ms100=0;//0.1s
	if (1) //!clk_invalid
	{
		v = inl(REG_TOY_READ0);//读取秒 默认0x0400 0000
		year = inl(REG_TOY_READ1);//读取年 默认0x14

		tms.tm_mon 	= ((v>>26)&0x3f);//26:31 6  月 默认0x0400 0000 1月 0~11
		tms.tm_mday = ((v>>21)&0x1f);	//21:25 5	日
		tms.tm_hour = (v>>16)&0x1f;	//16:20 5		时
		tms.tm_min 	= (v>>10)&0x3f;	//10:15 6      分
		tms.tm_sec 	= (v>>4)&0x3f;	//4:9 6   	秒
		tms.tm_sec100ms 		= v&0xf;		//0:3 4  	0.1秒

		tms.tm_wday = 0;
		tms.tm_year = year+ FROM_YEAR;
		tms.tm_isdst = tms.tm_gmtoff = 0;
		totalSecs = gmmktime(&tms);
	}
	else
	{
		printf("System RTC Unused.\n");
		return 0;
	}
	printf("%04d-%02d-%02d %02d:%02d:%02d %d\n",tms.tm_year,tms.tm_mon+1,tms.tm_mday,tms.tm_hour,tms.tm_min,tms.tm_sec,tms.tm_sec100ms);
	printf("from %d,the total time passed is %d(sec).\n",FROM_YEAR,totalSecs);
	return	(totalSecs);
}
FINSH_FUNCTION_EXPORT(date,print date);

int dateUpdata(int ac, char **argv)//dateUpdata 2018 10 23 13 23 56
{
	if(ac<7)
	{
		printf("dateUpdata 2018 10 23 13 23 56\n ");
		return 0;
	}
	unsigned int year,month,day,hour,min,sec;

	char * pt=argv[1];
	year=atoi(pt);
	if(year<FROM_YEAR||year>TO_YEAR)
	{
		printf("year error.\n");
		return -1;
	}

	pt=argv[2];
	month=atoi(pt);
	if(month>12||month<1)
	{
		printf("month error.\n");
		return -1;
	}

	pt=argv[3];
	day=atoi(pt);
	if(day>31||day<1)
	{
		printf("day error.\n");
		return -1;
	}

	pt=argv[4];
	hour=atoi(pt);
	if(hour>23||hour<0)
	{
		printf("hour error.\n");
		return -1;
	}
		
	pt=argv[5];
	min=atoi(pt);
	if(min>59||min<0)
	{
		printf("min error.\n");
		return -1;
	}
	
	pt=argv[6];
	sec=atoi(pt);
	if(sec>59||sec<0)
	{
		printf("sec error.\n");
		return -1;
	}

	unsigned int datas=0;
	
	datas=(sec<<4)|(min<<10)|(hour<<16)|((day)<<21)|((month-1)<<26);
	writel(datas, REG_TOY_WRITE0);
	datas=year-FROM_YEAR;
	writel(datas, REG_TOY_WRITE1);
	printf("write date success:%s-%s-%s %s:%s:%s .\n",argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
	return 1;
}
FINSH_FUNCTION_EXPORT(dateUpdata,dateUpdata);


int getDateSec(struct tm *tms)
{
	time_t totalSecs=0;
	unsigned int year=0,v=0;
	//unsigned int ms100=0;//0.1s
	if (1) //!clk_invalid
	{
		v = inl(REG_TOY_READ0);//读取秒 默认0x0400 0000
		year = inl(REG_TOY_READ1);//读取年 默认0x14

		tms->tm_mon 	= ((v>>26)&0x3f);//26:31 6  月 默认0x0400 0000 1月 0~11
		tms->tm_mday = ((v>>21)&0x1f);	//21:25 5	日
		tms->tm_hour = (v>>16)&0x1f;	//16:20 5		时
		tms->tm_min 	= (v>>10)&0x3f;	//10:15 6      分
		tms->tm_sec 	= (v>>4)&0x3f;	//4:9 6   	秒
		tms->tm_sec100ms 		= v&0xf;		//0:3 4  	0.1秒

		tms->tm_wday = 0;
		tms->tm_year = year+ FROM_YEAR;
		tms->tm_isdst = tms->tm_gmtoff = 0;
		totalSecs = gmmktime(tms);
	}
	else
	{
		printf("System RTC Unused.\n");
		return 0;
	}
	return	(totalSecs);
}

unsigned long maxAdd=0;
void test_mul_fun(void)
{
	struct tm tms;
	printf("maxAdd:%d\n",maxAdd);
	long timesP=getDateSec(&tms);
	unsigned long i=0,j=0,k=0;
	unsigned long maxi=maxAdd;
	unsigned long long sum=0;
	for(i=0;i<maxi;i++)
	{
		for(j=0;j<maxi;j++)
			for(k=0;k<maxi;k++)
				sum+=1;
	}
	printf("sum:%llu\n",sum);
	long timesP2=getDateSec(&tms);
	printf("it takes:%d sec\n",timesP2-timesP);
}
void test_mul(int argc,char**argv)
{
	maxAdd=atoi(argv[1]);
	thread_join_init("test_mul_fun",test_mul_fun,NULL,4096*10,0,100);//load网络内核 线程优先级要低于tcpip线程
}
FINSH_FUNCTION_EXPORT(test_mul,test_mul );


#if BUILD_TEST_FUN
#include "rtdef.h"

#include "queue.h"
#include "filesys.h"

extern struct rt_thread *rt_current_thread;

void pritnflonglong(void)
{
	char dst[32]="";
	
	long data=0x12345678;
	printf("pritnflong:0x%x\n",data);
	memset(dst,0,32);
	btoa(dst,data,16);
	printf("pritnflong s:%s\n",dst);
	
	off_t ldata=123;//0x12345678abcdef00;
	printf("pritnflonglong:%lld\n",ldata);
	memset(dst,0,32);
	btoa(dst,ldata,16);
	printf("pritnflonglong s:%s\n",dst);

	
}
FINSH_FUNCTION_EXPORT(pritnflonglong,print pritnflonglong);

//所有的测试函数
void hello(void)
{
	printf("hello world\n");
}
FINSH_FUNCTION_EXPORT(hello,print hello world);

int INIT_COMPONENT_test1_init(void)
{
	printf("INIT_COMPONENT_test1_init\r\n");
	return 0;
}
INIT_COMPONENT_EXPORT(INIT_COMPONENT_test1_init);

int INIT_COMPONENT_test2_init(void)
{
	printf("INIT_COMPONENT_test2_init\r\n");
	return 0;
}
INIT_COMPONENT_EXPORT(INIT_COMPONENT_test2_init);


long  rt_thread_delay(unsigned int tick);

void testthread01(void*userdata)
{
	int count=10000;
	while(1)
	{
		printf("testthread01:%d\n",count);
		count++;
		rt_thread_delay(2000);
	}
}

void testthread02(void*userdata)
{
	int count=20000;
	while(1)
	{
		printf("testthread02:%d\n",count);
		count++;
		rt_thread_delay(1000);
	}
}

void testthread03(void*userdata)
{
	int count=30000;
	while(1)
	{
		printf("testthread03:%d\n",count);
		count++;
		rt_thread_delay(500);
	}
}

void testthread04(void*userdata)//这个线程进入后 比它优先级低的线程永远没有机会获得运行权利
{
	int count=40000;
	while(1)
	{
		printf("testthread04:%d\n",count);
		count++;
		//rt_thread_delay(1000);//没有给延迟的话，线程tick到后 比它优先级低的线程永远没有机会获得运行权利
	}
}

void testthread15(void*userdata)//这个线程运行一段时间后会结束掉
{
	int count=150000;
	while(1)
	{
		printf("testthread15:%d\n",count);
		count++;
		if(count>150005)
			break;
		rt_thread_delay(1000);
	}
	printf("testthread15 end========================================.\n");
}

int testThread(void)
{
	thread_join_init("thread01",testthread01,NULL,2048,1,100);
	thread_join_init("thread02",testthread02,NULL,2048,2,100);
	thread_join_init("thread03",testthread03,NULL,2048,3,100);
	thread_join_init("thread15",testthread15,NULL,2048,5,100);
	
	//thread_join_init("thread04",testthread04,2048,NULL,4,100);
	
	return 0;
}
FINSH_FUNCTION_EXPORT(testThread,testThread in sys);

struct rt_semaphore sem_dst;//静态信号量，接收数据线程和处理数据线程抢占

void sem_testthread01(void*userdata)
{
	int count=10000;
	while(1)
	{
		//rt_sem_take(&sem_dst,RT_WAITING_FOREVER);//会阻塞 
		count++;
		if(count%10==0)
		{
			printf("sem_testthread01:%d,remain_tick:%d\n",count,rt_current_thread->remaining_tick);
			rt_sem_release(&sem_dst);
		}
		//rt_thread_delay(1000);
	}
}

void sem_testthread02(void*userdata)
{
	int count=20000;
	while(1)
	{
		rt_sem_take(&sem_dst,RT_WAITING_FOREVER);//会阻塞 
		printf("sem_testthread02:%d,remain_tick:%d\n",count,rt_current_thread->remaining_tick);
		count++;
		//rt_sem_release(&sem_dst);
	}
}

void sem_testthread03(void*userdata)
{
	int count=30000;
	while(1)
	{
		rt_sem_take(&sem_dst,RT_WAITING_FOREVER);//会阻塞 
		printf("sem_testthread03:%d,remain_tick:%d\n",count,rt_current_thread->remaining_tick);
		count++;
		//rt_sem_release(&sem_dst);
	}
}

void sem_testthread04(void*userdata)
{
	int count=40000;
	while(1)
	{
		rt_sem_take(&sem_dst,RT_WAITING_FOREVER);//会阻塞 
		printf("sem_testthread04:%d,remain_tick:%d\n",count,rt_current_thread->remaining_tick);
		count++;
		//rt_sem_release(&sem_dst);
	}
}

int sem_testThread(void)
{
	rt_sem_init(&sem_dst,"sem_dst",0,RT_IPC_FLAG_FIFO);

	thread_join_init("sem_testthread01",sem_testthread01,NULL,2048,3,53);
	thread_join_init("sem_testthread02",sem_testthread02,NULL,2048,2,50);
	thread_join_init("sem_testthread03",sem_testthread03,NULL,2048,2,50);
	thread_join_init("sem_testthread04",sem_testthread04,NULL,2048,2,50);
	
	
	return 0;
}



void cmd_testthread03(void*userdata)
{
	int count=30000;
	while(1)
	{
		printf("cmd_testthread03:%d\n",count);
		count++;
		rt_thread_delay(1000);
	}
}

void cmd_testthread01(void*userdata)
{
	int count=10000;
	char thread3Name[RT_NAME_MAX]="";
	while(1)
	{
		printf("cmd_testthread01:%d\n",count);
		count++;
		sprintf(thread3Name,"testthreadx%d",count);
		thread_join_init(thread3Name,cmd_testthread03,NULL,2048,2,50);
		rt_thread_delay(1000);
	}
}

void cmd_testthread02(void*userdata)
{
	int count=20000;
	while(1)
	{
		printf("cmd_testthread02:%d\n",count);
		count++;
		rt_thread_delay(1000);
	}
}



long test_jointhread(void)
{
	thread_join_init("cmd_testthread01",cmd_testthread01,NULL,2048,3,53);
	thread_join_init("cmd_testthread02",cmd_testthread02,NULL,2048,2,50);
	return 0;
}


FINSH_FUNCTION_EXPORT(test_jointhread,test_jointhread in sys);


void testMallocWhile(void)
{
	char*pt;
	while(1)
	{
		pt=malloc(80);
		printf("pt:%p\n",pt);
		free(pt);
		rt_thread_sleep(100);
	}
}

void test_malloc(void)
{

	thread_join_init("testMallocWhile",testMallocWhile,NULL,2048,14,50);
/*
	int i=1;
	while(1)
	{
		char *ppTest=(char*)malloc(4096*i-32+1);
		if(ppTest!=NULL)
			printf("malloc test ppTest:%d,:0x%08X\n",i,ppTest);
		i++;
		if(i>8)
			break;
	}
	
	i=0;
	while(1)
	{
		char *ppTest2=(char*)malloc(4096-32+i);
		if(ppTest2!=NULL)
			printf("malloc test ppTest2:%d,:0x%08X\n",i,ppTest2);
		i++;
		if(i>36)
			break;
	}
	*/
	
	//char *pp0=(char*)malloc(4096-32+1);
	//printf("malloc test pp0:0x%08X\n",pp0);
/*
	char *pp1=(char*)malloc(4096*3-32+1);
	printf("malloc test pp1:0x%08X\n",pp1);
*/

	/*
	char *pp0=(char*)malloc(1);
	printf("malloc test pp0:0x%08X\n",pp0);
	
	char *pp1=(char*)malloc(100);
	printf("malloc test pp1:0x%08X\npp1:",pp1);
	*/
/*
	char *pp2=(char*)malloc(4095);
	printf("\nmalloc test pp2:0x%08X\n",pp2);
*/
	/*
	char *pp4=(char*)malloc(4097);
	printf("malloc test pp4:0x%08X\n",pp4);

	*/
	
	/*
	char *pp5=(char*)malloc(4096);
	printf("malloc test pp5:0x%08X\n",pp5);
	free(pp5);
	free(pp2);
	printf("malloc free end\n");
	
	pp5=(char*)malloc(4096);
	printf("malloc test pp5:0x%08X\n",pp5);
	free(pp5);
	printf("malloc free end2\n");
	*/
	
	/*
	char *pp6=(char*)malloc(4096*2);
	printf("malloc test pp6:0x%08X\n",pp6);

	
	char *pp7=(char*)malloc(4096*2+100);
	printf("malloc test pp7:0x%08X\n",pp7);

	
	char *ppAlign=(char*)malloc_align(100,4096);
	printf("malloc test ppAlign:0x%08X\n",ppAlign);

	while(1)
	{
		char *ppWhile=(char*)malloc(4096);
		if(ppWhile==NULL)
			break;
		printf("malloc test ppWhile:0x%08X\n",ppWhile);
	}
	*/
	/*
	int i=0;
	char * memPt;
	for(i=1;i<4096;i++)
	{
		memPt=malloc(i);
		printf("i:%d,memPt:%p\n",i,memPt);
	}
	*/
}

FINSH_FUNCTION_EXPORT(test_malloc,test_malloc in sys);

void malloc_thread1(void*userdata)
{
	void *pt;
	int size=1532;
	while(1)
	{
		pt=malloc(size);
	}
}

void malloc_thread2(void*userdata)
{
	void *pt;
	int size=1534;
	while(1)
	{
		pt=malloc(size);
	}
}

void testMalloc(void)
{
	thread_join_init("malloc_thread1",malloc_thread1,NULL,2048,3,10);
	thread_join_init("malloc_thread2",malloc_thread2,NULL,2048,3,10);
	return 0;
}
FINSH_FUNCTION_EXPORT(testMalloc,testMalloc in sys);


void dead_testthread01(void*userdata)
{
	int count=10000;
	while(1)
	{
		printf("dead_testthread01:%d\n",count);
		count++;
		if(count>10000+10)
			break;
			
		rt_thread_delay(1000);
	}
}

void dead_testthread02(void*userdata)
{
	int count=20000;
	while(1)
	{
		printf("dead_testthread02:%d\n",count);
		count++;
		if(count>20000+15)
			break;
		rt_thread_delay(1000);
	}
}
long deadThread_jointhread(void)
{
	thread_join_init("dead_testthread01",dead_testthread01,NULL,4096,3,100);
	thread_join_init("dead_testthread02",dead_testthread02,NULL,4096,2,50);
	return 0;
}
FINSH_FUNCTION_EXPORT(deadThread_jointhread,deadThread_jointhread in sys);

extern rt_list_t rt_thread_dead;//死亡线程
void list_deadthread(void)
{
	struct rt_thread *dead_thread;
	if (!rt_list_isempty(&rt_thread_dead))
	rt_list_for_each_entry(dead_thread,&rt_thread_dead,tlist)
	{
		printf("priority:%d,thread name:%s\n",dead_thread->current_priority,dead_thread->name);
	}
}
FINSH_FUNCTION_EXPORT(list_deadthread,list_deadthread in sys);

extern unsigned int rt_interrupt_nest;
void show_irq_nest(void)//打印中断嵌套数值 >1说明中断嵌套了 1说明处在禁止中断状态
{
	printf("rt_interrupt_nest:%d\n",rt_interrupt_nest);
}
FINSH_FUNCTION_EXPORT(show_irq_nest,show_irq_nest in sys);


int readSpiFlashHex(int ac, char **argv)//读取rom中的hex数据
{
	char ret=0;
	unsigned int addr=0;
	unsigned int read_data_len=0;
	unsigned int readed_len=0;
	static char readbuf[512];
	printf("read_flash_data_cmd().\n");
	if(ac<3)
	{
		printf("useage: read_flash_data addr 32\n");
		return 0;
	}

	char * pt=argv[1]+2;//0x001234
	ret=panduan_hex(pt);
	if(ret==0)
	{
		printf("arg 1 not a hex addr.for example:0x12345678\n");
		return 0;
	}
	addr=str_to_hex(pt);
	read_data_len=atoi(argv[2]);
	printf("addr:0x%08X readlen:%d\n",addr,read_data_len);

	while(readed_len<read_data_len)
	{
		if(read_data_len-readed_len>512)
		{
			read_flash_data(addr,512,readbuf);
			show_hex(readbuf,512,addr);
			addr+=512;
			readed_len+=512;
		}
		else
		{
			read_flash_data(addr,read_data_len-readed_len,readbuf);
			show_hex(readbuf,read_data_len-readed_len,addr);
			break;
		}
	}
	
    return (1);
}
FINSH_FUNCTION_EXPORT(readSpiFlashHex,readSpiFlashHex in sys);

extern struct tftpFileStru tftpOpenfile;
void romBufShow(int argc ,char**argv)
{
	if(argc!=3)
	{
		printf("useage: romBufShow offsetHexAddr len \n");
		return;
	}
	
	char *argv1=malloc(strlen(argv[1])+1);
	memset(argv1,0,strlen(argv[1])+1);
	memcpy(argv1,argv[1],strlen(argv[1]));
	printf("argv1:%s\n",argv1);
	
	int offset=str_to_hex(argv1);
	int len=atoi(argv[2]);
	printf("offset:0x%08x,len:%d\n",offset,len);
	if(len<0)                                                                      
	{
		printf("len error <0.\n");
		return;
	}
	dataHexPrint2(tftpOpenfile.recvBuf,len,offset,"tftpOpenfile.recvBuf");
	free(argv1);
}
FINSH_FUNCTION_EXPORT(romBufShow,romBufShow tftp datas);

extern unsigned int tftpEveryReadSize;
void setTftpEverySize(int argc,char**argv)//设置tftp每次读取数据大小的测试函数
{
	if(argc!=2)
	{
		printf("useage: setTftpEverySize size \n");
		return;
	}
	tftpEveryReadSize=atoi(argv[1]);
}
FINSH_FUNCTION_EXPORT(setTftpEverySize,setTftpEverySize size);

#endif




