#include <stdio.h>
#include "rtdef.h"
#include "shell.h"

//struct finsh_shell shell_mem;
struct finsh_shell *shell;

//FINSH_FUNCTION_EXPORT申明的cmd指令存放区域
struct finsh_syscall *_syscall_table_begin  = NULL;
struct finsh_syscall *_syscall_table_end    = NULL;

typedef int (*cmd_function_t)(int argc, char **argv);
#define FINSH_ARG_MAX 10

#define msh_is_used()  1

int help(int argc, char **argv)
{
    printf("RT-Thread shell commands:\n");
    {
        struct finsh_syscall *index;

        for (index = _syscall_table_begin;
                index < _syscall_table_end;
                FINSH_NEXT_SYSCALL(index))
        {
			//printf("fun name:%s,desc:%s\n",index->name,index->desc);
			if (strncmp(index->name, "__cmd_", 6) != 0) 
				continue;
            printf("%-16s - %s\n", &index->name[6], index->desc);
        }
    }
    printf("\n");

    return 0;
}
FINSH_FUNCTION_EXPORT(help,help);

void msh_auto_complete_path(char *path)
{

}

int msh_exec_script(const char *cmd_line, int size)
{
	return 1;
}
void msh_auto_complete(char *prefix)
{
    int length, min_length;
    const char *name_ptr, *cmd_name;
    struct finsh_syscall *index;

    min_length = 0;
    name_ptr = RT_NULL;

    if (*prefix == '\0')
    {
        help(0, RT_NULL);
        return;
    }

    /* check whether a spare in the command */
    {
        char *ptr;
        ptr = prefix + strlen(prefix);
        while (ptr != prefix)
        {
            if (*ptr == ' ')
            {
                msh_auto_complete_path(ptr + 1);
                break;
            }
            ptr --;
        }
    }

    /* checks in internal command */
    {
        for (index = _syscall_table_begin; index < _syscall_table_end; FINSH_NEXT_SYSCALL(index))
        {
            /* skip finsh shell function */
            if (strncmp(index->name, "__cmd_", 6) != 0) 
				continue;

            cmd_name = (const char *) &index->name[6];
            if (strncmp(prefix, cmd_name, strlen(prefix)) == 0)
            {
                if (min_length == 0)
                {
                    /* set name_ptr */
                    name_ptr = cmd_name;
                    /* set initial length */
                    min_length = strlen(name_ptr);
                }

                length = str_common(name_ptr, cmd_name);
                if (length < min_length)
                    min_length = length;

                printf("%s\n", cmd_name);
            }
        }
    }

    /* auto complete string */
    if (name_ptr != NULL)
    {
        strncpy(prefix, name_ptr, min_length);
    }

    return ;
}


static char shell_handle_history(struct finsh_shell *shell)
{

    printf("\033[2K\r");
    printf("%s%s", FINSH_PROMPT, shell->line);
    return 0;
}

static void shell_push_history(struct finsh_shell *shell)
{
    if (shell->line_char_counter != 0)
    {
        /* push history */
        if (shell->history_count >= FINSH_HISTORY_LINES)
        {
            /* if current cmd is same as last cmd, don't push */
            if (memcmp(&shell->cmd_history[FINSH_HISTORY_LINES - 1], shell->line, FINSH_CMD_SIZE))
            {
                /* move history */
                int index;
                for (index = 0; index < FINSH_HISTORY_LINES - 1; index ++)
                {
                    memcpy(&shell->cmd_history[index][0],
                           &shell->cmd_history[index + 1][0], FINSH_CMD_SIZE);
                }
                memset(&shell->cmd_history[index][0], 0, FINSH_CMD_SIZE);
                memcpy(&shell->cmd_history[index][0], shell->line, shell->line_char_counter);

                /* it's the maximum history */
                shell->history_count = FINSH_HISTORY_LINES;
            }
        }
        else
        {
            /* if current cmd is same as last cmd, don't push */
            if (shell->history_count == 0 || memcmp(&shell->cmd_history[shell->history_count - 1], shell->line, FINSH_CMD_SIZE))
            {
                shell->current_history = shell->history_count;
                memset(&shell->cmd_history[shell->history_count][0], 0, FINSH_CMD_SIZE);
                memcpy(&shell->cmd_history[shell->history_count][0], shell->line, shell->line_char_counter);

                /* increase count and set current history position */
                shell->history_count ++;
            }
        }
    }
    shell->current_history = shell->history_count;
}

static void shell_auto_complete(char *prefix)
{
    printf("\n");
    if (msh_is_used() == 1)
    {
        msh_auto_complete(prefix);
    }
    printf("%s%s", FINSH_PROMPT, prefix);
}


static int msh_split(char *cmd, unsigned long length, char *argv[FINSH_ARG_MAX])
{
    char *ptr;
    unsigned long position;
    unsigned long argc;
    unsigned long i;

    ptr = cmd;
    position = 0; argc = 0;

    while (position < length)
    {
        /* strip bank and tab */
        while ((*ptr == ' ' || *ptr == '\t') && position < length)
        {
            *ptr = '\0';
            ptr ++; position ++;
        }

        if(argc >= FINSH_ARG_MAX)
        {
            printf("Too many args ! We only Use:\n");
            for(i = 0; i < argc; i++)
            {
                printf("%s ", argv[i]);
            }
            printf("\n");
            break;
        }

        if (position >= length) break;

        /* handle string */
        if (*ptr == '"')
        {
            ptr ++; position ++;
            argv[argc] = ptr; argc ++;

            /* skip this string */
            while (*ptr != '"' && position < length)
            {
                if (*ptr == '\\')
                {
                    if (*(ptr + 1) == '"')
                    {
                        ptr ++; position ++;
                    }
                }
                ptr ++; position ++;
            }
            if (position >= length) break;

            /* skip '"' */
            *ptr = '\0'; ptr ++; position ++;
        }
        else
        {
            argv[argc] = ptr;
            argc ++;
            while ((*ptr != ' ' && *ptr != '\t') && position < length)
            {
                ptr ++; position ++;
            }
            if (position >= length) break;
        }
    }

    return argc;
}
static cmd_function_t msh_get_cmd(char *cmd, int size)
{
    struct finsh_syscall *index;
    cmd_function_t cmd_func = RT_NULL;

    for (index = _syscall_table_begin;index < _syscall_table_end;FINSH_NEXT_SYSCALL(index))
    {
		//printf("fun name:%s,desc:%s\n",index->name,index->desc);
		if (strncmp(index->name, "__cmd_", 6) != 0) 
			continue;

        if (strncmp(&index->name[6], cmd, size) == 0 &&
                index->name[6 + size] == '\0')
        {
            cmd_func = (cmd_function_t)index->func;
            break;
        }
    }

    return cmd_func;
}

static int _msh_exec_cmd(char *cmd, unsigned long length, int *retp)
{
    int argc;
    unsigned long cmd0_size = 0;
    cmd_function_t cmd_func;
    char *argv[FINSH_ARG_MAX];

    RT_ASSERT(cmd);
    RT_ASSERT(retp);

    /* find the size of first command */
    while ((cmd[cmd0_size] != ' ' && cmd[cmd0_size] != '\t') && cmd0_size < length)
        cmd0_size ++;
    if (cmd0_size == 0)
        return -RT_ERROR;

    cmd_func = msh_get_cmd(cmd, cmd0_size);
    if (cmd_func == RT_NULL)
        return -RT_ERROR;

    /* split arguments */
    memset(argv, 0x00, sizeof(argv));
    argc = msh_split(cmd, length, argv);
    if (argc == 0)
        return -RT_ERROR;

    /* exec this command */
    *retp = cmd_func(argc, argv);
    return 0;
}

int msh_exec(char *cmd, unsigned long length)
{
    int cmd_ret;
    /* strim the beginning of command */
    while (*cmd  == ' ' || *cmd == '\t')
    {
        cmd++;
        length--;
    }

    if (length == 0)
        return 0;

    /* Exec sequence:
     * 1. built-in command
     * 2. module(if enabled)
     */
    if (_msh_exec_cmd(cmd, length, &cmd_ret) == 0)
    {
        return cmd_ret;
    }
    if (msh_exec_script(cmd, length) == 0)
    {
        return 0;
    }

    /* truncate the cmd at the first space. */
    {
        char *tcmd;
        tcmd = cmd;
        while (*tcmd != ' ' && *tcmd != '\0')
        {
            tcmd++;
        }
        *tcmd = '\0';
    }
    printf("%s: command not found.\n", cmd);
    return -1;
}

void shell_thread_entry(void *parameter)
{
    int ch;
    while (1)
    {
		ch = getchar();
        if (ch < 0)
        {
            continue;
        }
		//printf("input:0x%02X\n",ch);
		/*
		 *上下左右键特殊处理
		 * handle control key
		 * up key  : 0x1b 0x5b 0x41
		 * down key: 0x1b 0x5b 0x42
		 * right key:0x1b 0x5b 0x43
		 * left key: 0x1b 0x5b 0x44
		 */
		if (ch == 0x1b)
		{
			shell->stat = WAIT_SPEC_KEY;
			continue;
		}
		else if (shell->stat == WAIT_SPEC_KEY)
		{
			if (ch == 0x5b)
			{
				shell->stat = WAIT_FUNC_KEY;
				continue;
			}

			shell->stat = WAIT_NORMAL;
		}
		else if (shell->stat == WAIT_FUNC_KEY)
		{
			shell->stat = WAIT_NORMAL;

			if (ch == 0x41) /* up key */
			{
				/* prev history */
				if (shell->current_history > 0)
					shell->current_history --;
				else
				{
					shell->current_history = 0;
					continue;
				}

				/* copy the history command */
				memcpy(shell->line, &shell->cmd_history[shell->current_history][0],
					   FINSH_CMD_SIZE);
				shell->line_curpos = shell->line_char_counter = strlen(shell->line);
				shell_handle_history(shell);
				continue;
			}
			else if (ch == 0x42) /* down key */
			{
				/* next history */
				if (shell->current_history < shell->history_count - 1)
					shell->current_history ++;
				else
				{
					/* set to the end of history */
					if (shell->history_count != 0)
						shell->current_history = shell->history_count - 1;
					else
						continue;
				}

				memcpy(shell->line, &shell->cmd_history[shell->current_history][0],
					   FINSH_CMD_SIZE);
				shell->line_curpos = shell->line_char_counter = strlen(shell->line);
				shell_handle_history(shell);
				continue;
			}
			else if (ch == 0x44) /* left key */
			{
				if (shell->line_curpos)
				{
					printf("\b");
					shell->line_curpos --;
				}

				continue;
			}
			else if (ch == 0x43) /* right key */
			{
				if (shell->line_curpos < shell->line_char_counter)
				{
					printf("%c", shell->line[shell->line_curpos]);
					shell->line_curpos ++;
				}

				continue;
			}
		}

		/* received null or error */
		if (ch == '\0' || ch == 0xFF) 
			continue;
        else if (ch == '\t')//tab key 自动查找并补齐命令
        {
            int i;
            /* move the cursor to the beginning of line */
            for (i = 0; i < shell->line_curpos; i++)
                printf("\b");

            /* auto complete */
            shell_auto_complete(&shell->line[0]);
            /* re-calculate position */
            shell->line_curpos = shell->line_char_counter = strlen(shell->line);

            continue;
        }
        else if (ch == 0x7f || ch == 0x08)/* handle backspace key 删除键*/
        {
            /* note that shell->line_curpos >= 0 */
            if (shell->line_curpos == 0)
                continue;

            shell->line_char_counter--;
            shell->line_curpos--;

            if (shell->line_char_counter > shell->line_curpos)
            {
                int i;

                memmove(&shell->line[shell->line_curpos],
                           &shell->line[shell->line_curpos + 1],
                           shell->line_char_counter - shell->line_curpos);
                shell->line[shell->line_char_counter] = 0;

                printf("\b%s  \b", &shell->line[shell->line_curpos]);

                /* move the cursor to the origin position */
                for (i = shell->line_curpos; i <= shell->line_char_counter; i++)
                    printf("\b");
            }
            else
            {
                printf("\b \b");
                shell->line[shell->line_char_counter] = 0;
            }
            continue;
        }
		
		/* handle end of line, break */
		if (ch == '\r' || ch == '\n')//回车键
		{
			shell_push_history(shell);
			if (msh_is_used() == 1)
			{
				if (shell->echo_mode)
					printf("\n");
				msh_exec(shell->line, shell->line_char_counter);
			}
			printf(FINSH_PROMPT);
			memset(shell->line, 0, sizeof(shell->line));
			shell->line_curpos = shell->line_char_counter = 0;
			continue;
		}

        /* it's a large line, discard it */
        if (shell->line_char_counter >= FINSH_CMD_SIZE)
            shell->line_char_counter = 0;

        /* normal character */
        if (shell->line_curpos < shell->line_char_counter)
        {
            int i;

            memmove(&shell->line[shell->line_curpos + 1],
                       &shell->line[shell->line_curpos],
                       shell->line_char_counter - shell->line_curpos);
            shell->line[shell->line_curpos] = ch;
            if (shell->echo_mode)
                printf("%s", &shell->line[shell->line_curpos]);

            /* move the cursor to new position */
            for (i = shell->line_curpos; i < shell->line_char_counter; i++)
                printf("\b");
        }
        else
        {
            shell->line[shell->line_char_counter] = ch;
            if (shell->echo_mode)
                printf("%c", ch);
        }

        ch = 0;
        shell->line_char_counter ++;
        shell->line_curpos++;
        if (shell->line_char_counter >= FINSH_CMD_SIZE)
        {
            /* clear command line */
            shell->line_char_counter = 0;
            shell->line_curpos = 0;
        }

		
    }
}

void finsh_system_function_init(const void *begin, const void *end)
{
    _syscall_table_begin = (struct finsh_syscall *) begin;
    _syscall_table_end = (struct finsh_syscall *) end;
}

/*
//调试shell，显示shell内容
void showShellStatu(void)
{
	printf("shell->current_history:%d\n",shell->current_history);
	printf("shell->history_count:%d\n",shell->history_count);
	printf("shell->line_char_counter:%d\n",shell->line_char_counter);
	printf("shell->line_curpos:%d\n",shell->line_curpos);
	int i=0;
	for(i=0;i<FINSH_HISTORY_LINES;i++)
	{
		printf("i:%d,%s\n",i,shell->cmd_history[i]);
	}
	printf("shell->line:%s\n",shell->line);
	
}
FINSH_FUNCTION_EXPORT(showShellStatu,showShellStatu);
//*/

void shellInit(void)
{
    extern const int __fsymtab_start;	//系统编译完成后确定 变量在链接脚本ld.script中定义
    extern const int __fsymtab_end;		//系统编译完成后确定 变量在链接脚本ld.script中定义
	finsh_system_function_init(&__fsymtab_start, &__fsymtab_end);
	//printf("_syscall_table_begin:0x%08x,_syscall_table_end:0x%08x\n",_syscall_table_begin,_syscall_table_end);

	//shell=&shell_mem;
	shell = (struct finsh_shell *)malloc(sizeof(struct finsh_shell));
	//memset(shell,0,sizeof(struct finsh_shell));//不清零整个shell挺好的 重启后shell获取的内存位置不变的话，还能上下回溯
	shell->echo_mode=1;
	shell->prompt_mode=1;
	
	memset(shell->line,0,FINSH_CMD_SIZE);//把当前行清零吧 要不然挺麻烦的 比如运行reboot指令，重启后这个还在当前行
	shell->line_char_counter=0;
	shell->line_curpos=0;
	
	//showShellStatu();
}

