
#define RT_THREAD_PRIORITY_MAX 32
#define RT_TICK_PER_SECOND 1000

#define rt_inline					static __inline

#define SECTION(x)                  __attribute__((section(x)))
#define RT_USED 					__attribute__((used))
typedef int (*init_fn_t)(void);
#define INIT_COMPONENT_EXPORT(fn)       INIT_EXPORT(fn, "4")
#define INIT_EXPORT(fn, level)  RT_USED const init_fn_t __rt_init_##fn SECTION(".rti_fn."level) = fn



#define RT_TIMER_FLAG_NO_ACTIVATED         	0x0             /**< timer is no active */
#define RT_TIMER_FLAG_ACTIVATED         	0x1             /**< timer is active */
#define RT_TIMER_FLAG_PERIODIC          0x2             /**< periodic timer */

#define RT_IPC_FLAG_FIFO                0x00            /**< FIFOed IPC. @ref IPC. */
#define RT_WAITING_FOREVER              -1              /**< Block forever until get resource. */


#define RT_THREAD_INIT                  0x00                /**< Initialized status */
#define RT_THREAD_READY                 0x01                /**< Ready status */
#define RT_THREAD_SUSPEND               0x02                /**< Suspend status */
#define RT_THREAD_SLEEP		            0x03                /**< Sleep status */
#define RT_THREAD_RUNNING               0x04                /**< Running status */
#define RT_THREAD_CLOSE                 0x05                /**< Closed status */

//#define RT_THREAD_STAT_MASK             0x0f

#define RT_EOK                          0               /**< There is no error */
#define RT_ERROR                        1               /**< A generic error happens */
#define RT_ETIMEOUT                     2               /**< Timed out */
#define RT_EFULL                        3               /**< The resource is full */


#define RT_UINT32_MAX                   0xffffffff      /**< Maxium number of UINT32 */
#define RT_TICK_MAX                     RT_UINT32_MAX   /**< Maxium number of tick */
#define RT_NAME_MAX 					24

#define RT_NULL 						NULL

#define DebugPR printf("Debug info %s %s:%d\n",__FILE__ , __func__, __LINE__)
#define RT_ASSERT(EX)                                                         \
if (!(EX))                                                                    \
{                                                                             \
    rt_assert_handler(#EX, __FUNCTION__, __LINE__);                           \
}

#define rt_malloc(sz)			malloc(sz)
#define RT_KERNEL_MALLOC(sz)	rt_malloc(sz)
#define rt_memset				memset

#define rt_strncpy				strncpy


enum rt_object_class_type
{
    RT_Object_Class_Null   = 0,                         /**< The object is not used. */
    RT_Object_Class_Thread,                             /**< The object is a thread. */
	RT_Object_Class_Device, 							/**< The object is a device */
	
    //RT_Object_Class_Semaphore,                          /**< The object is a semaphore. */
    //RT_Object_Class_Mutex,                              /**< The object is a mutex. */
    //RT_Object_Class_Event,                              /**< The object is a event. */
    //RT_Object_Class_MailBox,                            /**< The object is a mail box. */
    //RT_Object_Class_MessageQueue,                       /**< The object is a message queue. */
    //RT_Object_Class_MemHeap,                            /**< The object is a memory heap */
    //RT_Object_Class_MemPool,                            /**< The object is a memory pool. */
    //RT_Object_Class_Timer,                              /**< The object is a timer. */
    //RT_Object_Class_Module,                             /**< The object is a module. */
    //RT_Object_Class_Unknown,                            /**< The object is unknown. */
    //RT_Object_Class_Static = 0x80                       /**< The object is a static object. */
    RT_Object_Class_Unknown
};


struct rt_list_node
{
    struct rt_list_node *next;                          /**< point to next node. */
    struct rt_list_node *prev;                          /**< point to prev node. */
};
typedef struct rt_list_node rt_list_t;                  /**< Type for lists. */

struct rt_object
{
    char       		name[RT_NAME_MAX];                       /**< name of kernel object */
    unsigned char 	type;                                    /**< type of kernel object */
    unsigned char 	flag;                                    /**< flag of kernel object */
    rt_list_t  		list;                                    /**< list node of kernel object */
};
typedef struct rt_object *rt_object_t;                  /**< Type for kernel objects. */

struct rt_object_information
{
    enum rt_object_class_type type;                     /**< object class type */
    rt_list_t                 object_list;              /**< object list */
    unsigned long             object_size;              /**< object size */
};


struct rt_semaphore
{
    //struct rt_ipc_object parent;                        /**< inherit from ipc_object */
    unsigned short          value;                         /**< value of semaphore. */
    unsigned short          reserved;                      /**< reserved field */

	unsigned char 			flag;							//dstling add
	rt_list_t  		suspend_thread_list;				//该信号量中被挂起的线程集合 
};
typedef struct rt_semaphore *rt_sem_t;

struct rt_mailbox
{
    //struct rt_ipc_object parent;                        /**< inherit from ipc_object */

    unsigned long          *msg_pool;                      /**< start address of message buffer */

    unsigned short          size;                          /**< size of message pool */

    unsigned short          entry;                         /**< index of messages in msg_pool */
    unsigned short          in_offset;                     /**< input offset of the message buffer */
    unsigned short          out_offset;                    /**< output offset of the message buffer */
	
	unsigned char 			flag;							//dstling add
	rt_list_t  			 suspend_thread_list;				//dstling add 接收被阻塞的线程链表 
	
    rt_list_t            suspend_sender_thread;         /**< sender thread suspended on this mailbox */
};
typedef struct rt_mailbox *rt_mailbox_t;

struct rt_timer
{
    //struct rt_object parent;                            /**< inherit from rt_object */
    char flag;			//激活与否标志

    rt_list_t  timelist;//row[RT_TIMER_SKIP_LIST_LEVEL];

    void (*timeout_func)(void *parameter);              /**< timeout function */
    void            *parameter;                         /**< timeout function's parameter */

    unsigned int        init_tick;                         /**< timer timeout tick */
    unsigned int        timeout_tick;                      /**< timeout tick */
};
typedef struct rt_timer *rt_timer_t;

struct rt_thread{
	char 			name[RT_NAME_MAX];
    unsigned char  	type;                                   /**< type of object */
    unsigned char  	flags;                                  /**< thread's flags */
    rt_list_t   	list;                                   /**< the object list */
	
    rt_list_t   	tlist;                                  /**< the thread list */
	rt_list_t		allList;								//用于存储所有的链表

	unsigned int	id;										//thread id dstling add
	
	unsigned char current_priority;
	unsigned char init_priority;
	
	unsigned int init_tick;
	unsigned int remaining_tick;

	unsigned int number_mask;

	void 	*parameter;
	
	char 	statu;// 0可运行RUN_ABLE_THREAD    1挂起暂停休眠SLEEP_THREAD 2线程任务结束DEAD_THREAD 可以清理删除

    void       *sp;                                     /**< stack point */
    void       *entry;                                  /**< entry */
    void       *stack_addr;                             /**< stack address */
    unsigned int stack_size;                             /**< stack size */

    long    error;                                  /**< error code */

	//char sleep_flag;
	//unsigned int sleep_tick_timeup;
	struct rt_timer thread_timer;

	
};
typedef struct rt_thread *rt_thread_t;

typedef struct rt_device *rt_device_t;
struct rt_device
{
    //struct rt_object          parent;                   /**< inherit from rt_object */

    //enum rt_device_class_type type;                     /**< device type */
    unsigned short               flag;                     /**< device flag */
    unsigned short                open_flag;                /**< device open flag */

    unsigned char                ref_count;                /**< reference count */
    unsigned char                device_id;                /**< 0 - 255 */

    /*  call back */
    long (*rx_indicate)(rt_device_t dev, unsigned long size);
    long (*tx_complete)(rt_device_t dev, void *buffer);


    /* common device interface */
    long  (*init)   (rt_device_t dev);
    long  (*open)   (rt_device_t dev, unsigned short oflag);
    long  (*close)  (rt_device_t dev);
    unsigned long (*read)   (rt_device_t dev, long pos, void *buffer, unsigned long size);
    unsigned long (*write)  (rt_device_t dev, long pos, const void *buffer, unsigned long size);
    long  (*control)(rt_device_t dev, int cmd, void *args);


    void	*user_data;                /**< device private data */
};



#define rt_container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

#define rt_list_entry(node, type, member) \
    rt_container_of(node, type, member)
#define rt_list_for_each_entry(pos, head, member) \
		for (pos = rt_list_entry((head)->next, typeof(*pos), member); \
			 &pos->member != (head); \
			 pos = rt_list_entry(pos->member.next, typeof(*pos), member))
			 
#define rt_list_for_each_entry_safe(pos, n, head, member) \
				for (pos = rt_list_entry((head)->next, typeof(*pos), member), \
					 n = rt_list_entry(pos->member.next, typeof(*pos), member); \
					 &pos->member != (head); \
					 pos = n, n = rt_list_entry(n->member.next, typeof(*n), member))



