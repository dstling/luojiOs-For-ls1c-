#include <buildconfig.h>

#if BUILD_TEST_FUN
#include <stdio.h>
#include "rtdef.h"
#include "shell.h"

#include "queue.h"
#include "filesys.h"

extern struct rt_thread *rt_current_thread;

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




