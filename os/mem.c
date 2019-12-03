#include <stdio.h>
#include <rtdef.h>

//#define RT_DEBUG
#ifdef RT_DEBUG
#define	MEM_DEBUG(format,args...) printf("MEM_DEBUG: " format "", ## args)
#define RT_DEBUG_MEM                   1
#define RT_DEBUG_LOG(type, message)                                           \
	do                                                                            \
	{                                                                             \
	    if (type)                                                                 \
	        printf message;                                                   \
	}                                                                             \
	while (0)
#else
	#define	MEM_DEBUG(format,args...)
	#define RT_DEBUG_LOG(type, message)
#endif

#define RT_DEBUG_CONTEXT_CHECK 1
#if RT_DEBUG_CONTEXT_CHECK
#define RT_DEBUG_NOT_IN_INTERRUPT                                             \
do                                                                            \
{                                                                             \
    rt_base_t level;                                                          \
    level = rt_hw_interrupt_disable();                                        \
    if (rt_interrupt_get_nest() != 0)                                         \
    {                                                                         \
        printf("Function[%s] shall not be used in ISR\n", __FUNCTION__);  \
        RT_ASSERT(0)                                                          \
    }                                                                         \
    rt_hw_interrupt_enable(level);                                            \
}                                                                             \
while (0)
#endif

#define RT_DEBUG_NOT_IN_INTERRUPT                                             \
do                                                                            \
{                                                                             \
    rt_base_t level;                                                          \
    level = rt_hw_interrupt_disable();                                        \
    if (rt_interrupt_get_nest() != 0)                                         \
    {                                                                         \
        printf("Function[%s] shall not be used in ISR\n", __FUNCTION__);  \
        RT_ASSERT(0)                                                          \
    }                                                                         \
    rt_hw_interrupt_enable(level);                                            \
}                                                                             \
while (0)


typedef unsigned short                  rt_uint16_t;    /**< 16bit unsigned integer type */
typedef unsigned int                    rt_uint32_t;    /**< 32bit unsigned integer type */
typedef long                            rt_base_t;      /**< Nbit CPU related date type */
typedef unsigned long                   rt_ubase_t;     /**< Nbit unsigned CPU related data type */
typedef rt_ubase_t                      rt_size_t;      /**< Type for size number */
typedef unsigned char                   rt_uint8_t;     /**<  8bit unsigned integer type */

#define RT_MEM_STATS

#define HEAP_MAGIC 0x1ea0
#define MIN_SIZE 12
#define RT_ALIGN_SIZE 8
#define RT_ALIGN(size, align)           (((size) + (align) - 1) & ~((align) - 1))
#define RT_ALIGN_DOWN(size, align)      ((size) & ~((align) - 1))

static rt_size_t used_mem, max_mem;
struct heap_mem
{
    /* magic and used flag */
    rt_uint16_t magic;
    rt_uint16_t used;

    rt_size_t next, prev;

    //rt_uint8_t thread[4];   /* thread name */
};

/** pointer to the heap: for alignment, heap_ptr is now a pointer instead of an array */
static rt_uint8_t *heap_ptr;

/** the last entry, always unused! */
static struct heap_mem *heap_end;


#define MIN_SIZE_ALIGNED     RT_ALIGN(MIN_SIZE, RT_ALIGN_SIZE)
#define SIZEOF_STRUCT_MEM    RT_ALIGN(sizeof(struct heap_mem), RT_ALIGN_SIZE)

static struct heap_mem *lfree;   /* pointer to the lowest free block */

static struct rt_semaphore heap_sem;
static rt_size_t mem_size_aligned;//可用内存总空间大小

static void plug_holes(struct heap_mem *mem)
{
    struct heap_mem *nmem;
    struct heap_mem *pmem;

    RT_ASSERT((rt_uint8_t *)mem >= heap_ptr);
    RT_ASSERT((rt_uint8_t *)mem < (rt_uint8_t *)heap_end);
    RT_ASSERT(mem->used == 0);

    /* plug hole forward */
    nmem = (struct heap_mem *)&heap_ptr[mem->next];
    if (mem != nmem && nmem->used == 0 && (rt_uint8_t *)nmem != (rt_uint8_t *)heap_end)
    {
        /* if mem->next is unused and not end of heap_ptr,
         * combine mem and mem->next
         */
        if (lfree == nmem)
        {
            lfree = mem;
        }
        mem->next = nmem->next;
        ((struct heap_mem *)&heap_ptr[nmem->next])->prev = (rt_uint8_t *)mem - heap_ptr;
    }

    /* plug hole backward */
    pmem = (struct heap_mem *)&heap_ptr[mem->prev];
    if (pmem != mem && pmem->used == 0)
    {
        /* if mem->prev is unused, combine mem and mem->prev */
        if (lfree == mem)
        {
            lfree = pmem;
        }
        pmem->next = mem->next;
        ((struct heap_mem *)&heap_ptr[mem->next])->prev = (rt_uint8_t *)pmem - heap_ptr;
    }
	
}


/**
 * @ingroup SystemInit
 *
 * This function will initialize system heap memory.
 *
 * @param begin_addr the beginning address of system heap memory.
 * @param end_addr the end address of system heap memory.
 */
extern char end_rom[];//系统编译完成后确定 变量在链接脚本ld.script中定义
extern unsigned int reserve_mem_space;
void rt_system_heap_init(void *begin_addr, void *end_addr)//用的这个
{
    struct heap_mem *mem;
	
    //printf("mem init, begin_addr address 0x%x,end_addr address 0x%x.\n",(rt_ubase_t)begin_addr, end_addr);

	//begin_addr=reserve_mem_space+1024*1024;//保留1M空间
	
	rt_ubase_t begin_align = RT_ALIGN((rt_ubase_t)begin_addr, RT_ALIGN_SIZE);
    rt_ubase_t end_align   = RT_ALIGN_DOWN((rt_ubase_t)end_addr, RT_ALIGN_SIZE);

    //RT_DEBUG_NOT_IN_INTERRUPT;

    /* alignment addr */
    if ((end_align > (2 * SIZEOF_STRUCT_MEM)) &&
        ((end_align - 2 * SIZEOF_STRUCT_MEM) >= begin_align))
    {
        /* calculate the aligned memory size */
		//可用内存总空间大小
        mem_size_aligned = end_align - begin_align - 2 * SIZEOF_STRUCT_MEM;
    }
    else
    {
        printf("mem init, error begin address 0x%x, and end address 0x%x\n",
                   (rt_ubase_t)begin_addr, (rt_ubase_t)end_addr);
        return;
    }

    /* point to begin address of heap */
    heap_ptr = (rt_uint8_t *)begin_align;

    RT_DEBUG_LOG(RT_DEBUG_MEM, ("mem init, heap begin address 0x%x, size %d,end address 0x%x.\n",
                                (rt_ubase_t)heap_ptr, mem_size_aligned,end_align));
    printf("mem init, heap begin address 0x%x, size %d,end address 0x%x,reserve_mem_space 0x%x,end_rom 0x%x\n",
		(rt_ubase_t)heap_ptr, mem_size_aligned,end_align,reserve_mem_space,end_rom);
	//MEM_DEBUG
	/*
	//测试内存
	memset(heap_ptr,'#',mem_size_aligned);
	char *ptChar=heap_ptr;
	int i=0;
	for(i=0;i<mem_size_aligned;i++)
	{
		if(ptChar[i]!='#')
			printf("mem addr:0x%08x error\n",&ptChar[i]);
	}
	*/
	
    /* initialize the start of the heap */
    mem        = (struct heap_mem *)heap_ptr;
    mem->magic = HEAP_MAGIC;
    mem->next  = mem_size_aligned + SIZEOF_STRUCT_MEM;
    mem->prev  = 0;
    mem->used  = 0;
	MEM_DEBUG("mem:0x%08x,heap_ptr->next:0x%08x\n",(rt_ubase_t)mem,(rt_ubase_t)mem->next);
#ifdef RT_USING_MEMTRACE
    rt_mem_setname(mem, "INIT");
#endif

    /* initialize the end of the heap */
    heap_end        = (struct heap_mem *)&heap_ptr[mem->next];//heap_ptr+mem->next
    heap_end->magic = HEAP_MAGIC;
    heap_end->used  = 1;
    heap_end->next  = mem_size_aligned + SIZEOF_STRUCT_MEM;
    heap_end->prev  = mem_size_aligned + SIZEOF_STRUCT_MEM;
	MEM_DEBUG("heap_end:0x%08x,heap_end->next:0x%08x\n",(rt_ubase_t)heap_end,(rt_ubase_t)heap_end->next);
#ifdef RT_USING_MEMTRACE
    rt_mem_setname(heap_end, "INIT");
#endif

    rt_sem_init(&heap_sem, "heap", 1, RT_IPC_FLAG_FIFO);

    /* initialize the lowest-free pointer to the start of the heap */
    lfree = (struct heap_mem *)heap_ptr;
}


/**
 * @addtogroup MM
 */

/**@{*/

/**
 * Allocate a block of memory with a minimum of 'size' bytes.
 *
 * @param size is the minimum size of the requested block in bytes.
 *
 * @return pointer to allocated memory or NULL if no free memory was found.
 */
 static int malloc_counter=0;
void *malloc(rt_size_t size)
{
    RT_DEBUG_NOT_IN_INTERRUPT;
    /* take memory semaphore */
    rt_sem_take(&heap_sem, RT_WAITING_FOREVER);
	
    rt_size_t ptr, ptr2;
    struct heap_mem *mem;
	struct heap_mem *mem2;
	malloc_counter++;

    if (size == 0)
        return RT_NULL;


    if (size != RT_ALIGN(size, RT_ALIGN_SIZE))
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("malloc size %d, but align to %d,malloc_counter:%d\n",
                                    size, RT_ALIGN(size, RT_ALIGN_SIZE),malloc_counter));
    else
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("malloc size %d,malloc_counter:%d\n", size,malloc_counter));

    /* alignment size */
    size = RT_ALIGN(size, RT_ALIGN_SIZE);

    if (size > mem_size_aligned)
    {
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("no memory\n"));

        return RT_NULL;
    }

    /* every data block must be at least MIN_SIZE_ALIGNED long */
    if (size < MIN_SIZE_ALIGNED)
        size = MIN_SIZE_ALIGNED;


    for (ptr = (rt_uint8_t *)lfree - heap_ptr;
         ptr < mem_size_aligned - size;
         ptr = ((struct heap_mem *)&heap_ptr[ptr])->next)
    {
		RT_DEBUG_LOG(RT_DEBUG_MEM,("ptr:0x%08x,mem_size_aligned - size:0x%08x,malloc_counter:%d\n",(rt_ubase_t)ptr,(rt_ubase_t)(mem_size_aligned - size),malloc_counter));
		mem = (struct heap_mem *)&heap_ptr[ptr];

        if ((!mem->used) && (mem->next - (ptr + SIZEOF_STRUCT_MEM)) >= size)
        {
            /* mem is not used and at least perfect fit is possible:
             * mem->next - (ptr + SIZEOF_STRUCT_MEM) gives us the 'user data size' of mem */

            if (mem->next - (ptr + SIZEOF_STRUCT_MEM) >=
                (size + SIZEOF_STRUCT_MEM + MIN_SIZE_ALIGNED))
            {
                /* (in addition to the above, we test if another struct heap_mem (SIZEOF_STRUCT_MEM) containing
                 * at least MIN_SIZE_ALIGNED of data also fits in the 'user data space' of 'mem')
                 * -> split large block, create empty remainder,
                 * remainder must be large enough to contain MIN_SIZE_ALIGNED data: if
                 * mem->next - (ptr + (2*SIZEOF_STRUCT_MEM)) == size,
                 * struct heap_mem would fit in but no data between mem2 and mem2->next
                 * @todo we could leave out MIN_SIZE_ALIGNED. We would create an empty
                 *       region that couldn't hold data, but when mem->next gets freed,
                 *       the 2 regions would be combined, resulting in more free memory
                 */
                ptr2 = ptr + SIZEOF_STRUCT_MEM + size;

                /* create mem2 struct */
                mem2       = (struct heap_mem *)&heap_ptr[ptr2];
                mem2->magic = HEAP_MAGIC;
                mem2->used = 0;
                mem2->next = mem->next;
                mem2->prev = ptr;
#ifdef RT_USING_MEMTRACE
                rt_mem_setname(mem2, "    ");
#endif

                /* and insert it between mem and mem->next */
                mem->next = ptr2;
                mem->used = 1;

                if (mem2->next != mem_size_aligned + SIZEOF_STRUCT_MEM)
                {
                    ((struct heap_mem *)&heap_ptr[mem2->next])->prev = ptr2;
                }
#ifdef RT_MEM_STATS
                used_mem += (size + SIZEOF_STRUCT_MEM);
                if (max_mem < used_mem)
                    max_mem = used_mem;
#endif
            }
            else
            {
                /* (a mem2 struct does no fit into the user data space of mem and mem->next will always
                 * be used at this point: if not we have 2 unused structs in a row, plug_holes should have
                 * take care of this).
                 * -> near fit or excact fit: do not split, no mem2 creation
                 * also can't move mem->next directly behind mem, since mem->next
                 * will always be used at this point!
                 */
                mem->used = 1;
#ifdef RT_MEM_STATS
                used_mem += mem->next - ((rt_uint8_t *)mem - heap_ptr);
                if (max_mem < used_mem)
                    max_mem = used_mem;
#endif
            }
            /* set memory block magic */
            mem->magic = HEAP_MAGIC;
#ifdef RT_USING_MEMTRACE
            if (rt_thread_self())
                rt_mem_setname(mem, rt_thread_self()->name);
            else
                rt_mem_setname(mem, "NONE");
#endif

            if (mem == lfree)
            {
                /* Find next free block after mem and update lowest free pointer */
                while (lfree->used && lfree != heap_end)
                    lfree = (struct heap_mem *)&heap_ptr[lfree->next];

                RT_ASSERT(((lfree == heap_end) || (!lfree->used)));
            }

            RT_ASSERT((rt_ubase_t)mem + SIZEOF_STRUCT_MEM + size <= (rt_ubase_t)heap_end);
            RT_ASSERT((rt_ubase_t)((rt_uint8_t *)mem + SIZEOF_STRUCT_MEM) % RT_ALIGN_SIZE == 0);
            RT_ASSERT((((rt_ubase_t)mem) & (RT_ALIGN_SIZE - 1)) == 0);

            RT_DEBUG_LOG(RT_DEBUG_MEM,
                         ("allocate memory at 0x%x, size: %d,malloc_counter:%d\n",
                          (rt_ubase_t)((rt_uint8_t *)mem + SIZEOF_STRUCT_MEM),
                          (rt_ubase_t)(mem->next - ((rt_uint8_t *)mem - heap_ptr)),malloc_counter));

           // RT_OBJECT_HOOK_CALL(rt_malloc_hook,
            //                    (((void *)((rt_uint8_t *)mem + SIZEOF_STRUCT_MEM)), size));

		   rt_sem_release(&heap_sem);
		   /* return the memory data except mem struct */
            return (rt_uint8_t *)mem + SIZEOF_STRUCT_MEM;
        }
    }
		 
	printf("malloc error!! return NULL,size:%d\n",size);
	list_mem();
	show_rt_current_thread_remain_tick();
	
	rt_sem_release(&heap_sem);

	/*
	list_mem();
	list_thread();
	while(1);
	*/
	
    return RT_NULL;
}


/**
 * This function will change the previously allocated memory block.
 *
 * @param rmem pointer to memory allocated by rt_malloc
 * @param newsize the required new size
 *
 * @return the changed memory block address
 */
void *realloc(void *rmem, rt_size_t newsize)
{
    rt_size_t size;
    rt_size_t ptr, ptr2;
    struct heap_mem *mem, *mem2;
    void *nmem;

    RT_DEBUG_NOT_IN_INTERRUPT;

    /* alignment size */
    newsize = RT_ALIGN(newsize, RT_ALIGN_SIZE);
    if (newsize > mem_size_aligned)
    {
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("realloc: out of memory\n"));

        return RT_NULL;
    }
    else if (newsize == 0)
    {
        free(rmem);
        return RT_NULL;
    }

    /* allocate a new memory block */
    if (rmem == RT_NULL)
        return rt_malloc(newsize);

    rt_sem_take(&heap_sem, RT_WAITING_FOREVER);

    if ((rt_uint8_t *)rmem < (rt_uint8_t *)heap_ptr ||
        (rt_uint8_t *)rmem >= (rt_uint8_t *)heap_end)
    {
        /* illegal memory */
        rt_sem_release(&heap_sem);
        return rmem;
    }

    mem = (struct heap_mem *)((rt_uint8_t *)rmem - SIZEOF_STRUCT_MEM);

    ptr = (rt_uint8_t *)mem - heap_ptr;
    size = mem->next - ptr - SIZEOF_STRUCT_MEM;
    if (size == newsize)
    {
        /* the size is the same as */
        rt_sem_release(&heap_sem);

        return rmem;
    }

    if (newsize + SIZEOF_STRUCT_MEM + MIN_SIZE < size)
    {
        /* split memory block */
#ifdef RT_MEM_STATS
        used_mem -= (size - newsize);
#endif

        ptr2 = ptr + SIZEOF_STRUCT_MEM + newsize;
        mem2 = (struct heap_mem *)&heap_ptr[ptr2];
        mem2->magic = HEAP_MAGIC;
        mem2->used = 0;
        mem2->next = mem->next;
        mem2->prev = ptr;
#ifdef RT_USING_MEMTRACE
        rt_mem_setname(mem2, "    ");
#endif
        mem->next = ptr2;
        if (mem2->next != mem_size_aligned + SIZEOF_STRUCT_MEM)
        {
            ((struct heap_mem *)&heap_ptr[mem2->next])->prev = ptr2;
        }

        plug_holes(mem2);

        rt_sem_release(&heap_sem);

        return rmem;
    }
    rt_sem_release(&heap_sem);

    /* expand memory */
    nmem = rt_malloc(newsize);
    if (nmem != RT_NULL) /* check memory */
    {
        memcpy(nmem, rmem, size < newsize ? size : newsize);
        free(rmem);
    }

    return nmem;
}


/**
 * This function will contiguously allocate enough space for count objects
 * that are size bytes of memory each and returns a pointer to the allocated
 * memory.
 *
 * The allocated memory is filled with bytes of value zero.
 *
 * @param count number of objects to allocate
 * @param size size of the objects to allocate
 *
 * @return pointer to allocated memory / NULL pointer if there is an error
 */
void *calloc(rt_size_t count, rt_size_t size)
{
    void *p;

    /* allocate 'count' objects of size 'size' */
    p = rt_malloc(count * size);

    /* zero the memory */
    if (p)
        rt_memset(p, 0, count * size);

    return p;
}


/**
 * This function will release the previously allocated memory block by
 * rt_malloc. The released memory block is taken back to system heap.
 *
 * @param rmem the address of memory which will be released
 */
void free(void *rmem)
{
    RT_DEBUG_NOT_IN_INTERRUPT;
    rt_sem_take(&heap_sem, RT_WAITING_FOREVER);
    struct heap_mem *mem;

    if (rmem == RT_NULL)
        return;


    RT_ASSERT((((rt_ubase_t)rmem) & (RT_ALIGN_SIZE - 1)) == 0);
    RT_ASSERT((rt_uint8_t *)rmem >= (rt_uint8_t *)heap_ptr &&
              (rt_uint8_t *)rmem < (rt_uint8_t *)heap_end);

    //RT_OBJECT_HOOK_CALL(rt_free_hook, (rmem));

    if ((rt_uint8_t *)rmem < (rt_uint8_t *)heap_ptr ||
        (rt_uint8_t *)rmem >= (rt_uint8_t *)heap_end)
    {
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("illegal memory\n"));

        return;
    }

    /* Get the corresponding struct heap_mem ... */
    mem = (struct heap_mem *)((rt_uint8_t *)rmem - SIZEOF_STRUCT_MEM);

    RT_DEBUG_LOG(RT_DEBUG_MEM,
                 ("release memory 0x%x, size: %d\n",
                  (rt_ubase_t)rmem,
                  (rt_ubase_t)(mem->next - ((rt_uint8_t *)mem - heap_ptr))));


    /* protect the heap from concurrent access */
    /* ... which has to be in a used state ... */
    if (!mem->used || mem->magic != HEAP_MAGIC)
    {
        printf("to free a bad data block:\n");
        printf("mem: 0x%08x, used flag: %d, magic code: 0x%04x\n", mem, mem->used, mem->magic);
    }
    RT_ASSERT(mem->used);
    RT_ASSERT(mem->magic == HEAP_MAGIC);
    /* ... and is now unused. */
    mem->used  = 0;
    mem->magic = HEAP_MAGIC;
#ifdef RT_USING_MEMTRACE
    rt_mem_setname(mem, "    ");
#endif

    if (mem < lfree)
    {
        /* the newly freed struct is now the lowest */
        lfree = mem;
    }

#ifdef RT_MEM_STATS
    used_mem -= (mem->next - ((rt_uint8_t *)mem - heap_ptr));
#endif

    /* finally, see if prev or next are free also */

    plug_holes(mem);
	
	
    rt_sem_release(&heap_sem);
	
}

void rt_memory_info(rt_uint32_t *total,
                    rt_uint32_t *used,
                    rt_uint32_t *max_used)
{
    if (total != RT_NULL)
        *total = mem_size_aligned;
    if (used  != RT_NULL)
        *used = used_mem;
    if (max_used != RT_NULL)
        *max_used = max_mem;
}
					
void list_mem(void)
{
	printf("total memory: %d\n", mem_size_aligned);
	printf("used memory : %d\n", used_mem);
	printf("maximum allocated memory: %d\n", max_mem);
}
#include <shell.h>
FINSH_FUNCTION_EXPORT(list_mem,list_mem    in sys);






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













int memcmp(const void *cs, const void *ct, unsigned long count)
{
    const unsigned char *su1, *su2;
    int res = 0;

    for (su1 = (const unsigned char *)cs, su2 = (const unsigned char *)ct; 0 < count; ++su1, ++su2, count--)
        if ((res = *su1 - *su2) != 0)
            break;

    return res;
}

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


