/**********************************************************************
* $Id$		lwipopts.h			2011-11-20
*//**
* @file		lwipopts.h
* @brief	LWIP option overrides
* @version	1.0
* @date		20. Nov. 2011
* @author	NXP MCU SW Application Team
*
* Copyright(C) 2011, NXP Semiconductor
* All rights reserved.
*
***********************************************************************
* Software that is described herein is for illustrative purposes only
* which provides customers with programming information regarding the
* products. This software is supplied "AS IS" without any warranties.
* NXP Semiconductors assumes no responsibility or liability for the
* use of the software, conveys no license or title under any patent,
* copyright, or mask work right to the product. NXP Semiconductors
* reserves the right to make changes in the software without
* notification. NXP Semiconductors also make no representation or
* warranty that such application will be suitable for the specified
* use without further testing or modification.
**********************************************************************/

#ifndef __LWIP_USER_OPT_H__
#define __LWIP_USER_OPT_H__

/* lwip debug */
//#define LWIP_DEBUG
#include <buildconfig.h>
#include <endian.h>

#define NO_SYS                      0
#define LWIP_NETCONN                1
#define LWIP_SOCKET                 1	//使用socket

//#define DEFAULT_THREAD_PRIO         1
#define TCPIP_THREAD_PRIO			TCPIP_THREAD_PRIO_GLO //tcpip线程优先级 tcpip_init static void tcpip_thread(void *arg) 在api/tcpip.c中 
#define TCPIP_THREAD_TICK			40		

#define DEFAULT_THREAD_STACKSIZE    (4096)
#define TCPIP_THREAD_STACKSIZE      DEFAULT_THREAD_STACKSIZE//tcpip线程堆栈区间大小

/* 32-bit alignment */
#define MEM_ALIGNMENT               4

#define MEM_LIBC_MALLOC             1	//1使用系统lib 0使用lwip自带的内存管理单元
#define MEM_USE_POOLS               0
/*
#define pbuf_alloc(layer, length,type)	({void*ret=pbuf_alloc2(layer, length,type);\
								printf("pbuf_alloc: %s,%s,%d,addr:%p,",__FILE__ , __func__, __LINE__,ret);\
								show_rt_current_thread_remain_tick();\
								ret;\
								})
#define pbuf_free(x)					do {printf("pbuf_free: %s,%s,%d,addr:%p\n",__FILE__ , __func__, __LINE__,x);pbuf_free2(x);}while(0);				
*/
#if !MEM_LIBC_MALLOC
extern unsigned char 			lwip_mem_pool[];//在ethernetif.c中定义	
#define LWIP_MEM_POOL_SIZE 		(10*1024*1024)	//内存池大小
#define LWIP_RAM_HEAP_POINTER	lwip_mem_pool 	
#endif
	
#if MEM_LIBC_MALLOC
void free (void *ptr);
#define mem_malloc                  malloc
#define mem_free                    free
#define mem_calloc                  calloc
#endif

#define MEMP_MEM_MALLOC             0	//1使用系统lib 1不能用，两个线程同时分配内存时会阻塞lwip就不能工作啦 0
#define MEMP_NUM_PBUF               128	//memp_malloc用 16
/*
#define memp_malloc(type)	({void*mem=memp_malloc2(type);\
								extern int tcpip_msg_counter;\
								tcpip_msg_counter++;\
								printf("malloc,%s,%d,addr:%p,type:%d,tcpip_msg_counter:%d ",__func__, __LINE__,mem,type,tcpip_msg_counter);\
								show_rt_current_thread_remain_tick();\
								mem;\
								})
#define memp_free(type,mem)	({\
								extern int tcpip_msg_counter;\
								tcpip_msg_counter--;\
								printf("free, %s,%d,addr:%p,type:%d,tcpip_msg_counter:%d ", __func__, __LINE__,mem,type,tcpip_msg_counter);\
								show_rt_current_thread_remain_tick();\
								memp_free2(type,mem);})				
*/


#define MEMP_NUM_NETCONN            8
#define MEMP_NUM_RAW_PCB            4
#define MEMP_NUM_UDP_PCB            4
#define MEMP_NUM_TCP_PCB            3
#define MEMP_NUM_TCP_SEG            40

#define PBUF_POOL_SIZE              4
#define PBUF_LINK_HLEN              16

//===========================================================================
extern unsigned char ip_addr_mem[];
extern unsigned char gateway_addr_mem[];
extern unsigned char mask_addr_mem[];
#define RT_LWIP_IPADDR 		ip_addr_mem				//"172.0.0.13"
#define RT_LWIP_GWADDR 		gateway_addr_mem		//"172.0.0.1"
#define RT_LWIP_MSKADDR 	mask_addr_mem			//"255.255.255.0"


#define LWIP_TCP                    1
#define TCP_QUEUE_OOSEQ             1
#define TCP_MSS                     1460
#define TCP_SND_BUF                 4096
#define TCP_SND_QUEUELEN            (4 * TCP_SND_BUF/TCP_MSS)
#define TCP_SNDLOWAT                (TCP_SND_BUF/2)
#define TCP_SNDQUEUELOWAT           TCP_SND_QUEUELEN/2
#define TCP_WND                     2048
#define TCP_MAXRTX                  12
#define TCP_SYNMAXRTX               4
#define TCP_TTL                     255
#define TCPIP_THREAD_NAME           "tcpip"

#define TCPIP_MBOX_SIZE             (8*3)		//tcpip线程邮箱大小 8貌似有点小啦？
#define DEFAULT_TCP_RECVMBOX_SIZE		10

#define LWIP_ARP                    1
#define ARP_TABLE_SIZE              10
#define ARP_QUEUEING                1

#define IP_FORWARD                  0
#define IP_REASSEMBLY               0
#define IP_FRAG                     0



#define LWIP_IGMP                   	1
#define LWIP_ICMP                   	1
#define ICMP_TTL                    255

#define LWIP_DNS                    	1


#ifdef LWIP_IGMP
//#include <stdlib.h>
#define LWIP_RAND                  rand
#endif



/* Use LWIP timers */
#define NO_SYS_NO_TIMERS				0

/* Need for memory protection */
#define SYS_LIGHTWEIGHT_PROT        (NO_SYS==0)


/* pbuf buffers in pool. In zero-copy mode, these buffers are
   located in peripheral RAM. In copied mode, they are located in
   internal IRAM */

 /* No padding needed */
#define ETH_PAD_SIZE                    0

#define IP_SOF_BROADCAST				1
#define IP_SOF_BROADCAST_RECV			1

/* ---------- Checksum options ---------- */
#ifdef RT_LWIP_USING_HW_CHECKSUM
#define CHECKSUM_GEN_IP                 0
#define CHECKSUM_GEN_UDP                0
#define CHECKSUM_GEN_TCP                0
#define CHECKSUM_GEN_ICMP               0
#define CHECKSUM_CHECK_IP               0
#define CHECKSUM_CHECK_UDP              0
#define CHECKSUM_CHECK_TCP              0
#define CHECKSUM_CHECK_ICMP             0
#endif
/* Use LWIP version of htonx() to allow generic functionality across
   all platforms. If you are using the Cortex Mx devices, you might
   be able to use the Cortex __rev instruction instead. */
#define LWIP_PLATFORM_BYTESWAP			0

/**
 * MEM_SIZE: the size of the heap memory. If the application will send
 * a lot of data that needs to be copied, this should be set high.
 */
#define MEM_SIZE						(5*1024*1024)
//使用自己的堆, 定义于sys_arch.c
//#define LWIP_RAM_HEAP_POINTER 			__lwip_heap  //dstling remove
//extern unsigned char __lwip_heap[];

/* Raw interface not needed */
#define LWIP_RAW                        0

/* DHCP is ok, UDP is required with DHCP */
#define LWIP_DHCP                       0
#define DHCP_DOES_ARP_CHECK         (LWIP_DHCP)

/* ---------- AUTOIP options ------- */
#define LWIP_AUTOIP                 0
#define LWIP_DHCP_AUTOIP_COOP       (LWIP_DHCP && LWIP_AUTOIP)

/* ---------- UDP options ---------- */
#define LWIP_UDP                        1

#define LWIP_UDPLITE                0
#define UDP_TTL                     255
#define DEFAULT_UDP_RECVMBOX_SIZE   1

/* ---------- RAW options ---------- */
#define DEFAULT_RAW_RECVMBOX_SIZE   1
#define DEFAULT_ACCEPTMBOX_SIZE     10

#define LWIP_STATS                  0
#define LINK_STATS						0
#define LWIP_STATS_DISPLAY              0

#define LWIP_POSIX_SOCKETS_IO_NAMES 1//dstling rtthread =0
#define LWIP_NETIF_API  1

#define MEMP_NUM_SYS_TIMEOUT       (LWIP_TCP + IP_REASSEMBLY + LWIP_ARP + (2*LWIP_DHCP) + LWIP_AUTOIP + LWIP_IGMP + LWIP_DNS + PPP_SUPPORT)
//#define MEMP_NUM_SYS_TIMEOUT            300

/* Hostname can be used */
#define LWIP_NETIF_HOSTNAME             1

#define LWIP_BROADCAST_PING				1




/* There are more *_DEBUG options that can be selected.
   See opts.h. Make sure that LWIP_DEBUG is defined when
   building the code to use debug. */
#define TCP_DEBUG                       LWIP_DBG_OFF
#define ETHARP_DEBUG                    LWIP_DBG_OFF
#define PBUF_DEBUG                      LWIP_DBG_OFF
#define IP_DEBUG                        LWIP_DBG_OFF
#define TCPIP_DEBUG                     LWIP_DBG_OFF
#define DHCP_DEBUG                      LWIP_DBG_OFF
#define UDP_DEBUG                       LWIP_DBG_OFF

/* This define is custom for the LPC EMAC driver. Enabled it to
   get debug messages for the driver. */
#define UDP_LPC_EMAC                    LWIP_DBG_OFF


#define IP_NAT                      0
#define LWIP_SNMP                   0

#define LWIP_HAVE_LOOPIF            0

#define BYTE_ORDER                  LITTLE_ENDIAN


#endif /* __LWIP_USER_OPT_H__ */
