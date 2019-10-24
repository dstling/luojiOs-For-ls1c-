#include<stdio.h>
extern struct rt_thread *rt_current_thread;


void dataHexPrint(void * datas, int dataLen,char * title)
{
	int i=0;
	int line=1;
	int line_count=0;
	printf("\n======================================\n");
	printf("%s. bytes data len:%d\n",title,dataLen);
	printf("%d	:",line);
	for(i=0;i<dataLen;i++,line_count++)
	{
		if(line_count==16)
		{
			line++;
			printf("\n%d	:",line);
			line_count=0;
		}
		printf("%02X ",((unsigned char *)datas)[i]);
	}
	printf("\n======================================\n");
	
}
char panduan_hex(char * p)
{
	int i=0;
	for(i=0;p[i]!='\0';i++)
	{
		switch(p[i])
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'A':case 'a':
		case 'B':case 'b':
		case 'C':case 'c':
		case 'D':case 'd':
		case 'E':case 'e':
		case 'F':case 'f':break;
		default:return 0;
		}
	}
	return 1;
}

unsigned int str_to_hex(char * p)
{
	unsigned int tempx=0;
	int i=0;
	for(i=0;p[i]!='\0';i++)
	{
		switch(p[i])
		{
		case '0':tempx<<=4;tempx+=0;break;
		case '1':tempx<<=4;tempx+=1;break;
		case '2':tempx<<=4;tempx+=2;break;
		case '3':tempx<<=4;tempx+=3;break;
		case '4':tempx<<=4;tempx+=4;break;
		case '5':tempx<<=4;tempx+=5;break;
		case '6':tempx<<=4;tempx+=6;break;
		case '7':tempx<<=4;tempx+=7;break;
		case '8':tempx<<=4;tempx+=8;break;
		case '9':tempx<<=4;tempx+=9;break;
		case 'A':case 'a':tempx<<=4;tempx+=10;break;
		case 'B':case 'b':tempx<<=4;tempx+=11;break;
		case 'C':case 'c':tempx<<=4;tempx+=12;break;
		case 'D':case 'd':tempx<<=4;tempx+=13;break;
		case 'E':case 'e':tempx<<=4;tempx+=14;break;
		case 'F':case 'f':tempx<<=4;tempx+=15;break;
		default:
			return -1;
		}
	}
	return tempx;
}

void show_hex(char * p,unsigned int len,unsigned int addr_offs)
{
	register long level;
///*	
	if(rt_current_thread!=NULL)//thread系统调度start
	{
		level = rt_hw_interrupt_disable();
	}
//*/
	int i=0,j=0,addr=0;
	printf("==========hex print==========\n");
	for(i=0;i<len;i++,addr++)
	{
		if(j==0)
			printf("%08X:",addr+addr_offs);
		j++;
		printf("%02X ",(unsigned char)p[i]);
		if(j==16)
		{
			printf("\n");
			j=0;
		}
	}
	printf("\n=============================\n");
///*
	if(rt_current_thread!=NULL)//thread系统调度未start
	{
		rt_hw_interrupt_enable(level);
	}
//*/
}

void dataHexPrint2(void * datas, int dataLen,int offset,char * title)
{
	int i=0;
	int line=offset;
	int line_count=0;
	printf("\n======================================\n");
	printf("%s. bytes data len:%d,offset:0x%08X\n",title,dataLen,offset);
	printf("%08X	:",line);
	for(i=offset;i<offset+dataLen;i++,line_count++)
	{
		if(line_count==16)
		{
			line+=16;
			printf("\n%08X	:",line);
			line_count=0;
		}
		printf("%02X ",((unsigned char *)datas)[i]);
	}
	printf("\n======================================\n");
	
}

void dotik(int rate, int use_ret)
{
	static	int             tik_cnt;
	static	const char      more_tiks[] = "|/-\\";
	static	const char     *more_tik;

	tik_cnt -= rate;
	if (tik_cnt > 0) {
		return;
	}
	tik_cnt = 256000;
	if (more_tik == 0) {
		more_tik = more_tiks;
	}
	if (*more_tik == 0) {
		more_tik = more_tiks;
	}
	if (use_ret) {
		printf (" %c\r", *more_tik);
	} else {
		printf ("\b%c", *more_tik);
	}
	more_tik++;
}

char * getstr(char *dst, int max)
{
	int c;
	char *p;

	/* get max bytes or upto a newline */

	for (p = dst, max--; max > 0; max--) 
	{
		c = getchar ();
		if(c==0)
			break;
		*p++ = c;
		if (c == '\n')
			break;
	}
	*p = 0;
	if (p == dst)
		return (0);
	return (p);
}

char * gets (char *p)
{
	char *s;
	int n;

	s = getstr (p, MAXLN);

	if (s == 0)
		return (0);
	n = strlen (p);
	if (n && p[n - 1] == '\n')
		p[n - 1] = 0;
	return (s);
}

