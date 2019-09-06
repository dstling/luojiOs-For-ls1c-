#include <stdio.h>
#include <types.h>

extern char		end[];//系统编译完成后确定 变量在链接脚本ld.script中定义
#define MEM_SIZE		32 			//MB 总物理内存大小
#define CLIENTPC 0x80100000			//1M的位置

#define USER_MEM_SIZE	10240			//KB malloc内存池大小 在malloc.c中被heaptop调用 end 至load elf（LOADADDRPC）指定的位置

//系统默认的堆顶 在init_heaptop被重新赋值
//不能超过kmem的位置 0x81100000 具体能不能超过多少 不能超过CLIENTPC？
char	*heapTop = end + 1024*USER_MEM_SIZE;
char	*heapEnd = end;
char	*heapPointer;//系统heap,sbrk函数动态内存指针
int		memorysize=0;//系统总物理内存 单位byte 应该是0x0200 0000

void init_heaptop(void)
{
	int memsz=MEM_SIZE;//内存大小
	memorysize = memsz > 256 ? 256 << 20 : memsz << 20;//32<<20/1024/1024=32  memorysize=32*1024*1024 (Byte)

	//pmon引导在这里限制了堆顶
	if (memorysize >= 0x2000000) //内存大于等于32MB 
	{	
		heapTop = 0x80800000;//pmon在这里限制了堆顶 白菜板8M 智龙板0x82000000  32M
	} 
	else 
	{
		heapTop = (unsigned int)(end + 65536) < CLIENTPC ? CLIENTPC : (end + 65536);
	}
	
	heapPointer = heapTop;
	heapEnd = end;
	printf("Available memory space from:0X%08X,to:0X%08X\n",heapEnd,heapTop);
}

//这里添加一个heap END判断
char *sbrk(int n)//从heaptop开始 从上往下分配内存
{
	char *top;
	top = heapTop;
	if (!top) 
		top = (char *)CLIENTPC;
	
	heapPointer -= n;

	//严禁越过堆底
	if(heapPointer<heapEnd)
	{
		printf("System Errors,heapPointer over heapEnd.\n");
		return NULL;
	}
	else
		return heapPointer;
}

#define ALLOC_PAGE_SIZE  (4096)//每次从系统堆中获取的最小堆大小
#define ADMIN_SIZE  (16)//一个内存块单元首尾被占用的空间，用于标识内存块地址 以及被占用与否标志
#define ALIGN_SIZE  (16)//这个是内存申请的最小单位 若内存块没被占用 头部被设定为BLOCK_FREE 1	,若被占用头部则已经被对齐为0

#define BLOCK_FREE  1
#define BLOCK_LAST  2

#define is_free(INFO)     ((((INFO) & 3) & BLOCK_FREE) == BLOCK_FREE)
#define is_last(INFO)     ((((INFO) & 3) & BLOCK_LAST) == BLOCK_LAST)
#define get_block_size(INFO)  (((INFO) & (~3)))
#define set_info(BLOCK)       (*((unsigned int *)(BLOCK)))
#define get_info(BLOCK)       (*((unsigned int *)(BLOCK)))
#define next_block(BLOCK)     ((BLOCK) + get_block_size (get_info ((BLOCK))) + ADMIN_SIZE)

//这里经过测试有个bug 修复  
void *_get_sys_mem (size_t req_size)
{
	size_t new_size = 0;
	unsigned char *new_heap = NULL, *block = NULL, *last_block = NULL;
	size_t align_size=0;
	//申请ALLOC_SIZE*N大小的空间
	if(req_size&(ALLOC_PAGE_SIZE-1))//取余
		align_size = req_size - (req_size & (ALLOC_PAGE_SIZE - 1))+ALLOC_PAGE_SIZE;
	else
		align_size=req_size+ALLOC_PAGE_SIZE;
	
	new_size=align_size;
/*	
	if (req_size > ALLOC_SIZE)// >4096
		new_size = req_size * 2;
	else
		new_size = ALLOC_SIZE;//4096
*/
	
	/* get memory from system */
	new_heap = (unsigned char *) sbrk (new_size);

	//printf("_get_sys_mem,req_size:%d,align_size:%d,new_heap:0x%08x\n",req_size,align_size,new_heap);
	
	if (new_heap == NULL)
		return NULL;

	block = (unsigned char *) new_heap;
	last_block = block + (new_size - ADMIN_SIZE);

	/*
	* Create two blocks, one to keep end-of-chunk information and
	* one for user allocations. The first 4bytes of each block are 
	* used to store size and flags. The last block of the chunk 
	* contains nothing but BLOCK_LAST flag. 
	*/
	//在第一个16字节空间，写入空余空间
	set_info (block) = ((new_size - (ADMIN_SIZE * 2)) | BLOCK_FREE);//2*ADMIN_SIZE=32byte空间被内存管理占用
	//在最后一个16字节空间,写入BLOCK_LAST标志
	set_info (last_block) = BLOCK_LAST;    /* last block is never free */

	return new_heap;
}

unsigned int _coalesce (unsigned char * block)
{
    unsigned char *nblock = NULL;
    unsigned int info = 0, ninfo = 0, nsize = 0, tsize = 0;

    info = get_info (block);
    tsize = get_block_size (info);
    
    if (is_last (info) || ! is_free (info))
      return tsize;

    for (;;)
      {
        nblock = next_block (block);
        ninfo = get_info (nblock);

        if (is_last (ninfo) || ! is_free (ninfo))
          return tsize;
            
        nsize = get_block_size (ninfo);
        tsize = tsize + nsize + ADMIN_SIZE;	/* use space required for bookkeeping as well */
        set_info (block) = tsize | BLOCK_FREE;
      }/* coalesce until we hit non-free block */

    return tsize;
}/* _coalesce */


static void *__smlib_heap_start = NULL;

void *malloc (size_t reqsize)
{
	size_t bsize = 0;
	unsigned int info = 0;
	unsigned char *block = NULL, *next_addr = NULL, *ret_addr = NULL;
	size_t align_size=0;//调整后申请内存的大小
	unsigned char new_chunk_reqed_flag=0;//重新从系统堆中获取空间标志
	
	register long level=rt_hw_interrupt_disable();//省点事 rtthread在这里使用了信号量 简单点 禁止中断拉倒
	
	/* align the size */
	if (reqsize & (ALIGN_SIZE - 1))//取余 不被整除
		align_size = ALIGN_SIZE - (reqsize & (ALIGN_SIZE - 1))+reqsize;
	else
		align_size=reqsize;
	//printf("malloc req modify reqsize:%d,align_size:%d\r\n",reqsize,align_size);

	/* get memory from system */

	if (__smlib_heap_start == NULL)
	{
		__smlib_heap_start = _get_sys_mem (align_size);
		new_chunk_reqed_flag=1;
		//printf("malloc __smlib_heap_start:0x%08X\r\n",__smlib_heap_start);
	}
	  
	if (__smlib_heap_start == NULL)
	{
		rt_hw_interrupt_enable(level);
		return NULL;
	}

	block = (unsigned char *) __smlib_heap_start;//定位到第一个block

	/* search for empty block */
	while(1)
	{
		info = get_info (block);//获取该block的大小信息
		if (is_free (info))
		{
			bsize = get_block_size (info);
			if (bsize < align_size)
				bsize = _coalesce (block);//聚合下散列的内存空间 尝试下
				
			//printf("malloc,reqsize:%d,bsize:%d,align_size:%d\n",reqsize,bsize,align_size);
			if(new_chunk_reqed_flag==1&&bsize < align_size)//已经从系统重新申请了新的block,但是空间还是不够
			{
				//重新调整申请大小 并对齐
				reqsize+=2*ADMIN_SIZE;//+32字节
				if (reqsize & (ALIGN_SIZE - 1))//取余 不被整除
					align_size = ALIGN_SIZE - (reqsize & (ALIGN_SIZE - 1))+reqsize;
				else
					align_size=reqsize;
				//printf("malloc req modify again reqsize:%d,align_size:%d\r\n",reqsize,align_size);
				
				block = next_block (block);//查找下一个block
				continue;
			}
			
			if (bsize >= align_size)//剩余的block size大于申请的空间 完美
			{
				ret_addr = block + ADMIN_SIZE;
				set_info (block) = align_size;  /* size of current block excuding bookkeeping */
				/* spilt this block only when blocksize is bigger than the request */
				if (bsize > align_size)
				{
					next_addr = ret_addr + align_size;  /* next block is at this address */
					set_info (next_addr) = (bsize - (align_size + ADMIN_SIZE))| BLOCK_FREE;  /* size remaining excuding bookkeeping */
				}
				rt_hw_interrupt_enable(level);
				return (void*) ret_addr;
			}
			else
			{
				block = next_block (block);//查找下一个block
				continue;
			}
		}
		else if (is_last (info))//到达最后一个block，需要从系统heap中重新申请内存块
		{
			unsigned char *new_chunk;
			/* check if we have another chunk linked to this block */
			new_chunk = (unsigned char *) get_block_size (info);
			if (new_chunk == NULL)
			{
				/* ask for more space */
				new_chunk = _get_sys_mem (align_size);
				if (new_chunk == NULL)
				{
					rt_hw_interrupt_enable(level);
					return NULL;
				}
				/* 
				* Keep the address of the next chunk in the
				* last block. This forms a chain of available
				* blocks.
				*/
				new_chunk_reqed_flag=1;//已经从系统重新申请了堆，标记下
				set_info (block) = (unsigned int) new_chunk | BLOCK_LAST;//倒数第二位置1 BLOCK_LAST
			}
			block = new_chunk;  /* we have a chunk linked to last block */
		}
		else
			block = next_block (block);//查找下一个block
	}
	rt_hw_interrupt_enable(level);
	return NULL;
}

void* malloc_align(unsigned int size, unsigned int align)//free还是个问题
{
    void *align_ptr;
    void *ptr;
    unsigned int align_size;

    /* align the alignment size to 4 byte */
    align = ((align + 0x03) & ~0x03);

    /* get total aligned size */
    align_size = ((size + 0x03) & ~0x03) + align;
	//printf("malloc_align align_size:%d\n",align_size);
    /* allocate memory block from heap */
    ptr = malloc(align_size);
	//printf("malloc_align ptr:0X%08X\n",ptr);
    if (ptr != NULL)
    {
         /* the allocated memory block is aligned */
        if (((unsigned int)ptr & (align - 1)) == 0)
        {
            align_ptr = (void *)((unsigned int)ptr + align);
        }
        else
        {
            align_ptr = (void *)(((unsigned int)ptr + (align - 1)) & ~(align - 1));
        }

        /* set the pointer before alignment pointer to the real pointer */
        *((unsigned int *)((unsigned int)align_ptr - sizeof(void *))) = (unsigned int)ptr;

        ptr = align_ptr;
    }

    return ptr;
}

void free (void *ptr)
{
	register long temp= rt_hw_interrupt_disable();

	unsigned char *block = NULL;
	unsigned int info = 0;

	if (ptr == NULL)
		return;

	/* mark current block as free */
	block = ((unsigned char *) ptr) - ADMIN_SIZE;
	info = get_info (block);
	set_info (block) = get_block_size (info) | BLOCK_FREE;

	/* coalesce adjacent free blocks */
	_coalesce (block);
	
    rt_hw_interrupt_enable(temp);

	return;
}/* free */

void *memmove(void *dest, const void *src, unsigned long n)
{
    char *tmp = (char *)dest, *s = (char *)src;

    if (s < tmp && tmp < s + n)
    {
        tmp += n;
        s += n;

        while (n--)
            *(--tmp) = *(--s);
    }
    else
    {
        while (n--)
            *tmp++ = *s++;
    }

    return dest;
}

int strncmp(const char *cs,const char *ct, unsigned int count)
{
	register signed char __res = 0;

	while (count) {
		if ((__res = *cs - *ct++) != 0 || !*cs++)
			break;
		count--;
	}

	return __res;
}

int str_common(const char *str1, const char *str2)
{
    const char *str = str1;

    while ((*str != 0) && (*str2 != 0) && (*str == *str2))
    {
        str ++;
        str2 ++;
    }

    return (str - str1);
}

int memcmp(const void *cs, const void *ct, unsigned long count)
{
    const unsigned char *su1, *su2;
    int res = 0;

    for (su1 = (const unsigned char *)cs, su2 = (const unsigned char *)ct; 0 < count; ++su1, ++su2, count--)
        if ((res = *su1 - *su2) != 0)
            break;

    return res;
}










