#include <stdio.h>
#include <types.h>
#include <shell.h>
#include <rtdef.h>
#include <buildconfig.h>

void hw_eth_init_thread_entry(void *parameter);

void shell_thread_entry(void *parameter);
void idle_thread_entry(void*userdata);//空闲线程 做一些死掉的线程清理工作
void lohci_attach_entry(void *parameter);

void ls1x_nand_init(void *parameter);

int rt_hw_eth_init(void)
{
	thread_join_init("hw_eth_init",hw_eth_init_thread_entry,NULL,2048,12,1000);
	return 0;
}
//FINSH_FUNCTION_EXPORT(rt_hw_eth_init,rt_hw_eth_init in sys);

int shell_init(void)
{
	shellInit();//shell初始化
	thread_join_init("shell",shell_thread_entry,NULL,2048,RT_THREAD_PRIORITY_MAX-1,8);//加入shell线程 给他最低的优先级吧 RT_THREAD_PRIORITY_MAX-1
	return 0;
}

int idle_init(void)
{
	thread_join_init("idle",idle_thread_entry,NULL,2048,RT_THREAD_PRIORITY_MAX-1,8);//至少加入一个空闲线程，系统管理用 给它倒数第1的优先级
	return 0;
}

void usb_init(void)
{
	thread_join_init("usb_init",lohci_attach_entry,NULL,4096*2,TCPIP_THREAD_PRIO_GLO+3,100);
	return 0;
}
FINSH_FUNCTION_EXPORT(usb_init,usb device attach);

void nand_init(void)
{
	thread_join_init("nand_init",ls1x_nand_init,NULL,4096,TCPIP_THREAD_PRIO_GLO+2,100);
	return 0;
}
FINSH_FUNCTION_EXPORT(nand_init,nand device attach);


