

#include <stdio.h>
#include <stdarg.h>
#include "../lib/ls1c_uart.h"


#define PRINTF_BUF_SIZE                 (2048)
static char printfbuf[PRINTF_BUF_SIZE];

extern struct rt_thread *rt_current_thread;

int printf (const char *fmt, ...)
{
	
	register long level;
///*	
	if(rt_current_thread!=NULL)//thread系统调度start
	{
		level = rt_hw_interrupt_disable();
	}
//*/

	int     len;
    va_list ap;

    va_start(ap, fmt);
	//memset(buf,'\0',PRINTF_BUF_SIZE);

    // 格式化字符串
    len = vsprintf (printfbuf, fmt, ap);
	if(len>PRINTF_BUF_SIZE-1)
		len=PRINTF_BUF_SIZE-1;
    // 调用龙芯1c库中的串口函数打印字符串
    uart_debug_print(printfbuf,len);
    
    va_end(ap);
	
///*
	if(rt_current_thread!=NULL)//thread系统调度未start
	{
		rt_hw_interrupt_enable(level);
	}
//*/
	
    return (len);
}
