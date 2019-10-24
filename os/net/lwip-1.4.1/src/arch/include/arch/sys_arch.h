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
 *
 */

#ifndef __RAW_ARCH_SYS_ARCH_H__
#define __RAW_ARCH_SYS_ARCH_H__

//#include <raw_api.h>
#include <rtdef.h>

#include "lwip/err.h"
#include "arch/cc.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

/* let sys.h use binary semaphores for mutexes */
/* 使用信号量来代替互斥锁 */
#define LWIP_COMPAT_MUTEX 1


/* HANDLE is used for sys_sem_t but we won't include windows.h */
/*
struct _sys_sem {
  //RAW_SEMAPHORE *sem;
  struct rt_semaphore *sem;
};

typedef struct _sys_sem sys_sem_t;
*/

typedef rt_sem_t sys_sem_t;

#define SYS_SEM_NULL NULL
//#define sys_sem_valid(sema) (((sema) != NULL) && ((sema)->sem != NULL))
//#define sys_sem_set_invalid(sema) ((sema)->sem = NULL)
/*
typedef struct Queue {
	unsigned short first;
	unsigned short count;
	unsigned short limit;
	unsigned char dat[1];
	} Queue;


struct lwip_mbox {
	//RAW_QUEUE	*queue ;
	Queue		*queue ;
};

typedef struct lwip_mbox sys_mbox_t;
*/
typedef rt_mailbox_t  sys_mbox_t;


#define SYS_MBOX_NULL NULL
//#define sys_mbox_valid(mbox) ((mbox != NULL) && ((mbox)->queue != NULL))
//#define sys_mbox_set_invalid(mbox) ((mbox)->queue = NULL)

//typedef RAW_U32 sys_thread_t;
//typedef unsigned int sys_thread_t;
typedef rt_thread_t sys_thread_t;

void *port_malloc(unsigned       int   size);
void   port_free(void *mem);

typedef long sys_prot_t;

/**********************************函数声明************************************/
err_t sys_sem_new(sys_sem_t *sem, u8_t count);
void sys_sem_free(sys_sem_t *sem);
void sys_sem_signal(sys_sem_t *sem);
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout);

#endif /* __RAW_ARCH_SYS_ARCH_H__ */

