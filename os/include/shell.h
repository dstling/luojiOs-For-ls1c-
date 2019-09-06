
#define FINSH_PROMPT     "MSH> "

#define FINSH_HISTORY_LINES 5
#define FINSH_CMD_SIZE 80

enum input_stat
{
    WAIT_NORMAL,
    WAIT_SPEC_KEY,
    WAIT_FUNC_KEY,
};

struct finsh_shell
{
    //struct rt_semaphore rx_sem;

    enum input_stat stat;

    unsigned char echo_mode:1;
    unsigned char prompt_mode: 1;

    unsigned short current_history;
    unsigned short history_count;

    char cmd_history[FINSH_HISTORY_LINES][FINSH_CMD_SIZE];

    char line[FINSH_CMD_SIZE];
    unsigned char line_position;
    unsigned char line_curpos;

};

typedef long (*syscall_func)(void);

struct finsh_syscall
{
	const char* 	name;		/* the name of system call */
	const char* 	desc;		/* description of system call */
	syscall_func func;		/* the function address of system call */
};


#define SECTION(x)                  __attribute__((section(x)))
#define RT_USED 					__attribute__((used))

#define FINSH_FUNCTION_EXPORT_CMD(name, cmd, desc)						\
	const char __fsym_##cmd##_name[] SECTION(".rodata.name") = #cmd;	\
	const char __fsym_##cmd##_desc[] SECTION(".rodata.name") = #desc;	\
	RT_USED const struct finsh_syscall __fsym_##cmd SECTION("FSymTab")= \
	{							\
		__fsym_##cmd##_name,	\
		__fsym_##cmd##_desc,	\
		(syscall_func)&name 	\
	};


#define FINSH_FUNCTION_EXPORT(command, desc)   \
    FINSH_FUNCTION_EXPORT_CMD(command, __cmd_##command, desc)

#define FINSH_NEXT_SYSCALL(index)  index++




