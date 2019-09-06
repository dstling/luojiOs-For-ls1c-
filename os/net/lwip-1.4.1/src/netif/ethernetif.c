/**
 * @file
 * Ethernet Interface Skeleton
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
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
 *
 */

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ethernetif" to replace it with
 * something that better describes your network interface.
 */
#include <rtdef.h>

#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "netif/etharp.h"

#include <ethernetif.h>

#define netifapi_netif_set_link_up(n)      netifapi_netif_common(n, netif_set_link_up, NULL)
#define netifapi_netif_set_link_down(n)    netifapi_netif_common(n, netif_set_link_down, NULL)

extern unsigned char default_MAC_addr[];

struct eth_tx_msg
{
    struct netif 	*netif;
    struct pbuf 	*buf;
};

static struct rt_mailbox eth_rx_thread_mb;
static struct rt_mailbox eth_tx_thread_mb;

#define RT_LWIP_ETHTHREAD_PRIORITY 14
#define RT_LWIP_ETHTHREAD_STACKSIZE 2048
#define RT_LWIP_ETHTHREAD_MBOX_SIZE 8

#define RT_ETHERNETIF_THREAD_PREORITY	RT_LWIP_ETHTHREAD_PRIORITY

static char eth_rx_thread_mb_pool[RT_LWIP_ETHTHREAD_MBOX_SIZE * 4];
//static char eth_rx_thread_stack[RT_LWIP_ETHTHREAD_STACKSIZE];

static char eth_tx_thread_mb_pool[RT_LWIP_ETHTHREAD_MBOX_SIZE * 4];
//static char eth_tx_thread_stack[RT_LWIP_ETHTHREAD_STACKSIZE];

static struct rt_thread* eth_tx_thread;
static struct rt_thread* eth_rx_thread;

/*
err_t etharp_output(struct netif *netif, struct pbuf *q, struct ip_addr *ipaddr)
{
     return 0;
}
*/
static err_t ethernetif_linkoutput(struct netif *netif, struct pbuf *p)
{
#ifndef LWIP_NO_TX_THREAD
    struct eth_tx_msg msg;
    struct eth_device* enetif;

	RT_ASSERT(netif != RT_NULL);
    enetif = (struct eth_device*)netif->state;

    /* send a message to eth tx thread */
    msg.netif = netif;
    msg.buf   = p;
    if (rt_mb_send(&eth_tx_thread_mb, (unsigned long) &msg) == RT_EOK)
    {
        /* waiting for ack */
        rt_sem_take(&(enetif->tx_ack), RT_WAITING_FOREVER);
    }
#else
    struct eth_device* enetif;

	RT_ASSERT(netif != RT_NULL);
    enetif = (struct eth_device*)netif->state;

	if (enetif->eth_tx(&(enetif->parent), p) != RT_EOK)
	{
		return ERR_IF;
	}
#endif
    return ERR_OK;
}

long eth_device_init(struct eth_device * dev, const char *name)
{
	
	unsigned int flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;//NETIF_FLAG_DHCP  NETIF_FLAG_IGMP
	
    struct netif* netif;

    netif = (struct netif*) malloc (sizeof(struct netif));
    if (netif == RT_NULL)
    {
        printf("malloc netif failed\n");
        return -RT_ERROR;
    }
	/*
	else
		printf("netif addr:0x%08X\n",netif);
	*/
    memset(netif, 0, sizeof(struct netif));
	
    dev->netif = netif;
    /* device flags, which will be set to netif flags when initializing */
    dev->flags = flags;
    /* link changed status of device */
    dev->link_changed = 0x00;
	
    //dev->parent.type = RT_Device_Class_NetIf;
    /* register to RT-Thread device manager */
    //rt_device_register(&(dev->parent), name, RT_DEVICE_FLAG_RDWR);
    
    rt_sem_init(&(dev->tx_ack), name, 0, RT_IPC_FLAG_FIFO);
	//printf("eth_device_init 1\n");

    /* set name */
    netif->name[0] = name[0];
    netif->name[1] = name[1];
	
    /* set hw address to 6 */
    netif->hwaddr_len 	= 6;
    /* maximum transfer unit */
    netif->mtu			= ETHERNET_MTU;
	
    /* get hardware MAC address */
	memcpy(netif->hwaddr,default_MAC_addr,6);
	
    /* set output */
	
    netif->output		= etharp_output;
    netif->linkoutput	= ethernetif_linkoutput;
	//printf("eth_device_init 2\n");

	
    /* if tcp thread has been started up, we add this netif to the system */
	///*
	//这个时候tcpip thread还未创建
    if (0)//rt_thread_find("tcpip") != RT_NULL
    {
        struct ip_addr ipaddr, netmask, gw;

#if LWIP_DHCP
        if (dev->flags & NETIF_FLAG_DHCP)
        {
            IP4_ADDR(&ipaddr, 0, 0, 0, 0);
            IP4_ADDR(&gw, 0, 0, 0, 0);
            IP4_ADDR(&netmask, 0, 0, 0, 0);
        }
        else
#endif
        {
            ipaddr.addr = inet_addr(RT_LWIP_IPADDR);
            gw.addr = inet_addr(RT_LWIP_GWADDR);
            netmask.addr = inet_addr(RT_LWIP_MSKADDR);
        }

        //netifapi_netif_add(netif, &ipaddr, &netmask, &gw, dev, eth_netif_device_init, tcpip_input);
    }
	//*/
}

long eth_device_linkchange(struct eth_device* dev, char up)
{
    register long level;

    RT_ASSERT(dev != RT_NULL);

    level = rt_hw_interrupt_disable();
    dev->link_changed = 0x01;
    if (up == 1)
        dev->link_status = 0x01;
    else
        dev->link_status = 0x00;
    rt_hw_interrupt_enable(level);

    /* post message to ethernet thread */
    return rt_mb_send(&eth_rx_thread_mb, (unsigned long)dev);
}

///*
long eth_device_ready(struct eth_device* dev)
{
    if (dev->netif)
        // post message to Ethernet thread 
        return rt_mb_send(&eth_rx_thread_mb, (unsigned long)dev);
    else
        return ERR_OK;  //netif is not initialized yet, just return. 
}
//*/


/* Ethernet Rx Thread */
static void eth_rx_thread_entry(void* parameter)
{
    struct eth_device* device;

    while (1)
    {
        if (rt_mb_recv(&eth_rx_thread_mb, (unsigned long*)&device, RT_WAITING_FOREVER) == RT_EOK)
        {
            struct pbuf *p;
			//printf("eth_rx_thread_entry rt_mb_recv,name:%s\n",device->name);

            /* check link status */
            if (device->link_changed)
            {
                int status;
                unsigned int level;

                level = rt_hw_interrupt_disable();
                status = device->link_status;
                device->link_changed = 0x00;
                rt_hw_interrupt_enable(level);

                if (status)
                    netifapi_netif_set_link_up(device->netif);
                else
                    netifapi_netif_set_link_down(device->netif);
            }

            /* receive all of buffer */
            while (1)
            {
            	if(device->eth_rx == RT_NULL) break;
            	
                p = device->eth_rx(device);
                if (p != RT_NULL)
                {
                    /* notify to upper layer */
                    if( device->netif->input(p, device->netif) != ERR_OK )
                    {
                        LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: Input error\n"));
                        pbuf_free(p);
                        p = NULL;
                    }
                }
                else break;
            }
        }
        else
        {
            LWIP_ASSERT("Should not happen!\n",0);
        }
    }
}

/* Ethernet Tx Thread */
static void eth_tx_thread_entry(void* parameter)
{
    struct eth_tx_msg* msg;

    while (1)
    {
        if (rt_mb_recv(&eth_tx_thread_mb, (unsigned long*)&msg, RT_WAITING_FOREVER) == RT_EOK)
        {
            struct eth_device* enetif;

            RT_ASSERT(msg->netif != RT_NULL);
            RT_ASSERT(msg->buf   != RT_NULL);

            enetif = (struct eth_device*)msg->netif->state;
            if (enetif != RT_NULL)
            {
                /* call driver's interface */
                if (enetif->eth_tx(enetif, msg->buf) != RT_EOK)
                {
                    /* transmit eth packet failed */
                }
            }

            /* send ACK */
			//printf("eth_tx_thread_entry rt_sem_release,name:%s\n",enetif->name);
            rt_sem_release(&(enetif->tx_ack));
        }
    }
}

int eth_system_device_init(void)
{
    long result = RT_EOK;

    /* initialize Rx thread. */
    /* initialize mailbox and create Ethernet Rx thread */
    result = rt_mb_init(&eth_rx_thread_mb, "erxmb",
                        &eth_rx_thread_mb_pool[0], sizeof(eth_rx_thread_mb_pool)/4,
                        RT_IPC_FLAG_FIFO);
    RT_ASSERT(result == RT_EOK);

    eth_rx_thread = thread_join_init("erx", eth_rx_thread_entry, RT_NULL,
                            RT_LWIP_ETHTHREAD_STACKSIZE,
                            RT_ETHERNETIF_THREAD_PREORITY, 16);

    /* initialize Tx thread */
    /* initialize mailbox and create Ethernet Tx thread */
    result = rt_mb_init(&eth_tx_thread_mb, "etxmb",
                        &eth_tx_thread_mb_pool[0], sizeof(eth_tx_thread_mb_pool)/4,
                        RT_IPC_FLAG_FIFO);
    RT_ASSERT(result == RT_EOK);

    eth_tx_thread = thread_join_init("etx", eth_tx_thread_entry, RT_NULL,
                            RT_LWIP_ETHTHREAD_STACKSIZE,
                            RT_ETHERNETIF_THREAD_PREORITY, 16);
    return 0;
}


void list_if(void)
{
    unsigned long index;
    struct netif * netif;

    //rt_enter_critical();

    netif = netif_list;

    while( netif != RT_NULL )
    {
        printf("network interface: %c%c%s\n",
                   netif->name[0],
                   netif->name[1],
                   (netif == netif_default)?" (Default)":"");
        printf("MTU: %d\n", netif->mtu);
        printf("MAC: ");
        for (index = 0; index < netif->hwaddr_len; index ++)
            printf("%02x ", netif->hwaddr[index]);
        printf("\nFLAGS:");
        if (netif->flags & NETIF_FLAG_UP) printf(" UP");
        else printf(" DOWN");
        if (netif->flags & NETIF_FLAG_LINK_UP) printf(" LINK_UP");
        else printf(" LINK_DOWN");
        if (netif->flags & NETIF_FLAG_DHCP) printf(" DHCP");
        if (netif->flags & NETIF_FLAG_POINTTOPOINT) printf(" PPP");
        if (netif->flags & NETIF_FLAG_ETHARP) printf(" ETHARP");
        if (netif->flags & NETIF_FLAG_IGMP) printf(" IGMP");
        printf("\n");
        printf("ip address: %s\n", ipaddr_ntoa(&(netif->ip_addr)));
        printf("gw address: %s\n", ipaddr_ntoa(&(netif->gw)));
        printf("net mask  : %s\n", ipaddr_ntoa(&(netif->netmask)));
        printf("\r\n");

        netif = netif->next;
    }

#if LWIP_DNS
    {
        struct ip_addr ip_addr;

        for(index=0; index<DNS_MAX_SERVERS; index++)
        {
            ip_addr = dns_getserver(index);
            printf("dns server #%d: %s\n", index, ipaddr_ntoa(&(ip_addr)));
        }
    }
#endif /**< #if LWIP_DNS */

    //rt_exit_critical();
}
#include "shell.h"

FINSH_FUNCTION_EXPORT(list_if,list_if in sys);





