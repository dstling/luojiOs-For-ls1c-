#include <stdio.h>
#include "rtdef.h"
#include "shell.h"

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

void test_malloc(void)
{

	
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
	char *pp2=(char*)malloc(4095);
	printf("\nmalloc test pp2:0x%08X\n",pp2);

	/*
	char *pp4=(char*)malloc(4097);
	printf("malloc test pp4:0x%08X\n",pp4);

	*/
	char *pp5=(char*)malloc(4096);
	printf("malloc test pp5:0x%08X\n",pp5);
	free(pp5);
	free(pp2);
	printf("malloc free end\n");
	
	pp5=(char*)malloc(4096);
	printf("malloc test pp5:0x%08X\n",pp5);
	free(pp5);
	printf("malloc free end2\n");
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
}

FINSH_FUNCTION_EXPORT(test_malloc,test_malloc in sys);

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


void dataHexPrint(void * datas, int dataLen,char * title)
{
	int i=0;
	int line=1;
	int line_count=0;
	printf("\n======================================\n");
	printf("%s. bytes data len:%d\n",title,dataLen);
	printf("%d	:",line);
	for(i=0;i<dataLen;i++,line_count++)
	{
		if(line_count==16)
		{
			line++;
			printf("\n%d	:",line);
			line_count=0;
		}
		printf("%02X ",((unsigned char *)datas)[i]);
	}
	printf("\n======================================\n");
	
}

static int showThreadSpMem(int argc, char **argv)
{
	if(argc!=2)
	{
		printf("showThreadSpMem threadID\n");
		return 1;
	}
	unsigned int id=atoi(argv[1]);
	rt_thread_t findedThread=find_thread_by_id(id);
	printf("finded thread name:%s\n",findedThread->name);
	dataHexPrint(findedThread->stack_addr,findedThread->stack_size,findedThread->name);
	return 0;
}
FINSH_FUNCTION_EXPORT(showThreadSpMem,showThreadSpMem in sys);



