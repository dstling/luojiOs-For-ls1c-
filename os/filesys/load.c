
#include <stdio.h>
#include "queue.h"
#include "filesys.h"
#include <shell.h>
#include <rtdef.h>


//============================================================================
extern long LOAD_THREAD;		//在context_gcc.S中定义 原因是load的时候地址靠前安全
extern long LOAD_THREAD_STACK;	//在context_gcc.S中定义
extern struct rt_thread *rt_current_thread;

rt_thread_t go_thread=NULL;
void* loadStack;
void* loadEntry;

static char loadelf_buf[2048];
void do_loadelf(void*userdata)
{
	int offsetN=0;
	long ep;
	char* loadelfAddr=getenv("loadelfAddr");
	if(loadelfAddr!=0)
	{
		int fd=open(loadelfAddr,0|O_NONBLOCK);//mode  OpenLoongsonLib1c.bin
		printf("loading...\n");
		if(fd>-1)
		{
			ep=load_elf (fd, loadelf_buf, &offsetN,0 );
			printf ("\nEntry address is %08x\n", ep);
			close(fd);
			loadEntry=ep;//指定入口
			loadStack=&LOAD_THREAD_STACK;//指定load的地址使用的stack
		}
		else
			printf("tftp Open error.\n");
	}
	else
		printf("set loadelfAddr tftp://x.x.x.x/elfFormatFilename");
	
	while(1)
		rt_thread_delay(1000);
}

void loadelf(int argc,char**argv)
{
	thread_join_init("do_loadelf",do_loadelf,NULL,4092,16,50);
}
FINSH_FUNCTION_EXPORT(loadelf,loadelf tftp addr);


void go(void)
{
	if(loadEntry!=0)
	{
		register long level;
		if(rt_current_thread!=NULL)//说明此时线程已经运作
		{
			level=rt_hw_interrupt_disable();
			rt_current_thread=NULL;//无论如何要停止线程调度
		}
		go_thread=&LOAD_THREAD;
		go_thread->sp=rt_hw_stack_init(loadEntry,NULL,loadStack,NULL);
		
		printf("loadEntry:0x%08x\n",loadEntry);
		printf("loadStack:0x%08x\n",loadStack);
		printf("go_thread->sp:0x%08x\n",go_thread->sp);
		
		rt_hw_context_switch_to((unsigned long)&go_thread->sp);//转入load内核 下面是生是死和luojios无关了
	}
	
}
FINSH_FUNCTION_EXPORT(go,go load kernel);





