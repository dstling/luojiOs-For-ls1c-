/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *         Simon Goldschmidt
 *
 */

//#include <stdlib.h>
#include <stdio.h> /* sprintf() for task names */
//#include <rtdef.h> /
#include <lwip/opt.h>
#include <lwip/arch.h>
#include <lwip/stats.h>
#include <lwip/debug.h>
#include <lwip/sys.h>
#include <lwip/ip_addr.h>
#include <lwip/tcpip.h>
#include <lwip/init.h>
#include <lwip/inet.h>

#include <ethernetif.h>

#include <lwip/netifapi.h>
/******************************************************************************/
/*
 * port_malloc port_free是为了自己管理内存所用函数，非LwIP移植必须
 */
/******************************************************************************/
/*
#define LWIP_BYTE_POOL_SIZE (1*1024*1024)
static RAW_BYTE_POOL_STRUCT lwip_byte_pool;
static RAW_U8 __attribute__((aligned(4))) __lwip_byte_pool[LWIP_BYTE_POOL_SIZE];
*/
/*
void *port_malloc(RAW_U32 size)
{
	void *ret;

	raw_byte_allocate(&lwip_byte_pool, &ret, size);

	return ret;
}

void port_free(void *free_ptr)
{
	raw_byte_release(&lwip_byte_pool, free_ptr);
}
*/
/******************************************************************************/
/*
#ifdef LWIP_RAM_HEAP_POINTER
//使用自己的lwip堆, 配置于lwipots.h
unsigned char __attribute__((aligned(4))) __lwip_heap[MEM_SIZE];
#endif
*/
//static int msg_fake_null;

extern struct eth_device  this_eth_dev;
static err_t netif_device_init(struct netif *netif)// 被tcpip_init_done_callback调用 在lwip tcpip thread中被调用 
{
    ///*
    struct eth_device *ethif;
    ethif = (struct eth_device *)netif->state;
    if (ethif != RT_NULL)
    {
        //rt_device_t device;
        // get device object 
        //device = (rt_device_t) ethif;
        //if (rt_device_init(device) != RT_EOK)//设备初始化 在synMain中定义 eth_init();
		if(init_eth()!= RT_EOK)
		{
            return ERR_IF;
        }
        // copy device flags to netif flags 
        netif->flags = ethif->flags;
        return ERR_OK;
    }
    return ERR_IF;
	//*/
}

/*
 * Initialize the ethernetif layer and set network interface device up
 */
void assert_printf(char *msg, int line, char *file)
{
	printf("%s,%d,%s\n",msg,line,file);
}

static void tcpip_init_done_callback(void *arg)
{
    //rt_device_t device;
    struct eth_device *ethif;
    struct ip_addr ipaddr, netmask, gw;
    //struct rt_list_node* node;
    //struct rt_object* object;
    //struct rt_object_information *information;

    LWIP_ASSERT("invalid arg.\n",arg);
	
    IP4_ADDR(&gw, 0,0,0,0);
    IP4_ADDR(&ipaddr, 0,0,0,0);
    IP4_ADDR(&netmask, 0,0,0,0);

	ethif=&this_eth_dev;
	LOCK_TCPIP_CORE();
	netif_add(
		ethif->netif, 
		&ipaddr, 
		&netmask, 
		&gw,
		(void*)ethif, netif_device_init, tcpip_input);
	

	if (netif_default == RT_NULL)
		netif_set_default(ethif->netif);
	
#if LWIP_DHCP
	if (ethif->flags & NETIF_FLAG_DHCP)
	{
		/* if this interface uses DHCP, start the DHCP client */
		dhcp_start(ethif->netif);
	}
	else
#endif

	{
		/* set interface up */
		netif_set_up(ethif->netif);
	}

	if (!(ethif->flags & ETHIF_LINK_PHYUP))
	{

		netif_set_link_up(ethif->netif);
	}

	UNLOCK_TCPIP_CORE();
	
    rt_sem_release((rt_sem_t)arg);
	//printf("tcpip_init_done_callback end\n");
}

int lwip_system_init(void)
{
    long rc;
    struct rt_semaphore done_sem;

    /* set default netif to NULL */
    netif_default = RT_NULL;

    rc = rt_sem_init(&done_sem, "done", 0, RT_IPC_FLAG_FIFO);

    if (rc != RT_EOK)
    {
        LWIP_ASSERT("Failed to create semaphore", 0);

        return -1;
    }

    tcpip_init(tcpip_init_done_callback, (void *)&done_sem);

    // waiting for initialization done 
    if (rt_sem_take(&done_sem, RT_WAITING_FOREVER) != RT_EOK)
    {
		rt_sem_detach(&done_sem);
        return -1;
    }
    rt_sem_detach(&done_sem);
	
    // set default ip address 
#if !LWIP_DHCP
    if (netif_default != RT_NULL)
    {
        struct ip_addr ipaddr, netmask, gw;

        ipaddr.addr = inet_addr(RT_LWIP_IPADDR);
        gw.addr = inet_addr(RT_LWIP_GWADDR);
        netmask.addr = inet_addr(RT_LWIP_MSKADDR);
        netifapi_netif_set_addr(netif_default, &ipaddr, &netmask, &gw);
    }
#endif
	printf("lwIP-%d.%d.%d initialized!\n", LWIP_VERSION_MAJOR, LWIP_VERSION_MINOR, LWIP_VERSION_REVISION);

	return 0;
}



u32_t sys_now()
{
	return sys_tick_get()* (1000 / RT_TICK_PER_SECOND);
}

//register long globle_level ;
sys_prot_t sys_arch_protect(void)
{
	//raw_disable_sche();
	register long temp=rt_hw_interrupt_disable();
	return temp;
}

void sys_arch_unprotect(sys_prot_t pval)
{
	//raw_enable_sche();
	rt_hw_interrupt_enable(pval);
}

void sys_init()
{
	/*
	raw_byte_pool_create(&lwip_byte_pool,				// byte内存池对象 
						 (RAW_U8 *)"lwip_byte_pool",	// byte内存池名称 
						 __lwip_byte_pool,				// 内存池起始地址 
						 LWIP_BYTE_POOL_SIZE);			// 内存池大小     
	*/
	
}

int sys_sem_valid(sys_sem_t *sem)
{
    return (int)(*sem);
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
    *sem = RT_NULL;
}


err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    static unsigned short counter = 0;
    char tname[RT_NAME_MAX];
    sys_sem_t tmpsem;

    //RT_DEBUG_NOT_IN_INTERRUPT;

    //rt_snprintf(tname, RT_NAME_MAX, "%s%d", SYS_LWIP_SEM_NAME, counter);
    counter ++;

    tmpsem = rt_sem_create(tname, count, RT_IPC_FLAG_FIFO);
    if (tmpsem == RT_NULL)
        return ERR_MEM;
    else
    {
        *sem = tmpsem;

        return ERR_OK;
    }
}

void sys_sem_free(sys_sem_t *sem)
{
	rt_sem_delete(*sem);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    long ret;
    s32_t t;
    u32_t tick;

    //RT_DEBUG_NOT_IN_INTERRUPT;

    /* get the begin tick */
    tick = sys_tick_get();
    if (timeout == 0)
        t = RT_WAITING_FOREVER;
    else
    {
        /* convert msecond to os tick */
        if (timeout < (1000/RT_TICK_PER_SECOND))
            t = 1;
        else
            t = timeout / (1000/RT_TICK_PER_SECOND);
    }

    ret = rt_sem_take(*sem, t);

    if (ret == -RT_ETIMEOUT)
        return SYS_ARCH_TIMEOUT;
    else
    {
        if (ret == RT_EOK)
            ret = 1;
    }

    /* get elapse msecond */
    tick = sys_tick_get() - tick;

    /* convert tick to msecond */
    tick = tick * (1000 / RT_TICK_PER_SECOND);
    if (tick == 0)
        tick = 1;

    return tick;
}

void sys_sem_signal(sys_sem_t *sem)
{
	rt_sem_release(*sem);
}


sys_thread_t sys_thread_new(const char *name, lwip_thread_fn function, void *arg, int stacksize, int prio)
{
    return thread_join_init(name, function, arg, stacksize, prio, 20);//tick=20
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    static unsigned short counter = 0;
    char tname[RT_NAME_MAX];
    sys_mbox_t tmpmbox;

    //RT_DEBUG_NOT_IN_INTERRUPT;
    //rt_snprintf(tname, RT_NAME_MAX, "%s%d", SYS_LWIP_MBOX_NAME, counter);
    //counter ++;

    tmpmbox = rt_mb_create(tname, size, RT_IPC_FLAG_FIFO);
    if (tmpmbox != RT_NULL)
    {
        *mbox = tmpmbox;

        return ERR_OK;
    }

    return ERR_MEM;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    //RT_DEBUG_NOT_IN_INTERRUPT;
    rt_mb_delete(*mbox);
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    //RT_DEBUG_NOT_IN_INTERRUPT;

    rt_mb_send_wait(*mbox, (unsigned long)msg, RT_WAITING_FOREVER);

    //return;
}


err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (rt_mb_send(*mbox, (unsigned long)msg) == RT_EOK)
        return ERR_OK;

    return ERR_MEM;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox ,void **msg, u32_t timeout)
{
    long ret;
    s32_t t;
    u32_t tick;

    //RT_DEBUG_NOT_IN_INTERRUPT;

    /* get the begin tick */
    tick = sys_tick_get();

    if(timeout == 0)
        t = RT_WAITING_FOREVER;
    else
    {
        /* convirt msecond to os tick */
        if (timeout < (1000/RT_TICK_PER_SECOND))
            t = 1;
        else
            t = timeout / (1000/RT_TICK_PER_SECOND);
    }

    ret = rt_mb_recv(*mbox, (unsigned long *)msg, t);

    if(ret == -RT_ETIMEOUT)
        return SYS_ARCH_TIMEOUT;
    else
    {
        LWIP_ASSERT("rt_mb_recv returned with error!", ret == RT_EOK);
    }

    /* get elapse msecond */
    tick = sys_tick_get() - tick;

    /* convert tick to msecond */
    tick = tick * (1000 / RT_TICK_PER_SECOND);
    if (tick == 0)
        tick = 1;

    return tick;
}


u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    int ret;

    ret = rt_mb_recv(*mbox, (unsigned long *)msg, 0);

    if(ret == -RT_ETIMEOUT)
        return SYS_ARCH_TIMEOUT;
    else
    {
        if (ret == RT_EOK) 
            ret = 1;
    }

    return ret;
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
    return (int)(*mbox);
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
    *mbox = RT_NULL;
}


/*
 * Prints an assertion messages and aborts execution.
 */
void sys_assert( const char *pcMessage )
{
	(void) pcMessage;

	for (;;)
	{
	}
}



