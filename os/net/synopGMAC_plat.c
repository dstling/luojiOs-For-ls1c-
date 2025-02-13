/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2017-08-24     chinesebear  first version
 */

 
#include "synopGMAC_plat.h"
#include "synopGMAC_Dev.h"
//#include <rthw.h>
//#include <rtthread.h>

#define cache_op(op,addr)                       \
	    __asm__ __volatile__(                       \
				    "   .set    push                    \n" \
				    "   .set    noreorder               \n" \
				    "   .set    mips3\n\t               \n" \
				    "   cache   %0, %1                  \n" \
				    "   .set    pop                 \n" \
				    :                               \
				    : "i" (op), "R" (*(unsigned char *)(addr)))

#if defined(CONFIG_CPU_LOONGSON2)
#define Hit_Invalidate_I    0x00
#else
#define Hit_Invalidate_I    0x10
#endif
#define Hit_Invalidate_D    0x11
#define CONFIG_SYS_CACHELINE_SIZE   32
#define Hit_Writeback_Inv_D 0x15

void flush_cache(unsigned long start_addr, unsigned long size)
{
	unsigned long lsize = CONFIG_SYS_CACHELINE_SIZE;
	unsigned long addr = start_addr & ~(lsize - 1); 
	unsigned long aend = (start_addr + size - 1) & ~(lsize - 1); 

	while (1) {
		cache_op(Hit_Writeback_Inv_D, addr);
		cache_op(Hit_Invalidate_I, addr);
		if (addr == aend)
			break;
		addr += lsize;
	}   
}

extern void flush_cache(unsigned long start_addr, unsigned long size);
dma_addr_t __attribute__((weak)) gmac_dmamap(unsigned long va,u32 size)
{
	return VA_TO_PA (va);
	//return UNCACHED_TO_PHYS(va);
}

/**
  * This is a wrapper function for Memory allocation routine. In linux Kernel 
  * it it kmalloc function
  * @param[in] bytes in bytes to allocate
  */

void *plat_alloc_memory(u32 bytes) 
{
	void *buf = (void*)rt_malloc((u32)bytes);
	if(buf!=0)
		flush_cache((unsigned long)buf, bytes);
	return buf;
}

/**
* This is a wrapper function for consistent dma-able Memory allocation routine. 
* In linux Kernel, it depends on pci dev structure
* @param[in] bytes in bytes to allocate
*/
void *plat_alloc_consistent_dmaable_memory(synopGMACdevice *pcidev, u32 size, u32 *addr) 
{
	void *buf;
	//buf = (void*)rt_malloc((u32)(size+16));
	buf =malloc(size);//(void*)malloc_align(size,0x100);//malloc(size);// (void*)malloc_align(size,0x100);
	DEBUG_MES("plat_alloc_consistent_dmaable_memory buf addr:0X%08X\n",buf);
	//CPU_IOFlushDCache( buf,size, SYNC_W);
	unsigned long i = (unsigned long)buf;
	//printf("size = %d\n", size);
	//printf("bufaddr = %p\n", buf);
	/*
	rt_kprintf("i%%16 == %d\n", i%16);
	if(i%16 == 8){
		i += 8;
	}
	else if(i%16 == 4){
		i += 12;
	}
	else if(i%16 == 12){
		i += 4;
	}
	*/
	if(i!=0)
		flush_cache(i, size);
	*addr =gmac_dmamap(i, size);
	buf = (unsigned char *)CACHED_TO_UNCACHED(i);
//	rt_kprintf("bufaddr = %p\n", buf);
	return buf;
}

/**
  * This is a wrapper function for freeing consistent dma-able Memory.
  * In linux Kernel, it depends on pci dev structure
  * @param[in] bytes in bytes to allocate
  */

//void plat_free_consistent_dmaable_memory(void * addr) 
void plat_free_consistent_dmaable_memory(synopGMACdevice *pcidev, u32 size, void * addr,u32 dma_addr) 
{
	rt_free((void*)PHYS_TO_CACHED(UNCACHED_TO_PHYS(addr)));
 return;
}

/**
  * This is a wrapper function for Memory free routine. In linux Kernel 
  * it it kfree function
  * @param[in] buffer pointer to be freed
  */
void plat_free_memory(void *buffer) 
{
	rt_free(buffer);
	return ;
}

dma_addr_t plat_dma_map_single(void *hwdev, void *ptr, u32 size)
{
    unsigned long addr = (unsigned long) ptr;
	//CPU_IOFlushDCache(addr,size, direction);
	if(addr!=0)
		flush_cache(addr, size);
	return gmac_dmamap(addr, size);
}

/**
  * This is a wrapper function for platform dependent delay 
  * Take care while passing the argument to this function 
  * @param[in] buffer pointer to be freed
  */
void plat_delay(u32 delay)
{
	while (delay--);
	return;
}


