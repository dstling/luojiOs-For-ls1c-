#include<stdio.h>
#include<types.h>
#include<ctype.h>
#include<filesys.h>

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

void dataHexPrint2(void * datas2, int dataLen,int offset,char * title)
{
	int i=0;
	int line=offset;
	int line_count=0;
	unsigned char * datasPt=datas2;
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
		printf("%02X ",((unsigned char *)datasPt)[i]);
	}
	printf("\n======================================\n");
	
}

void dotik(int rate, int use_ret)
{
	static	int             tik_cnt;
	static	const char      more_tiks[] = "|/-\\";
	static	const char     *more_tik;

	tik_cnt -= rate;
	if (tik_cnt > 0) 
		return;
	
	tik_cnt = 256000;
	if (more_tik == 0) 
		more_tik = more_tiks;
	
	if (*more_tik == 0) 
		more_tik = more_tiks;
	
	if (use_ret) 
		printf (" %c\r", *more_tik);
	 else 
		printf ("\b%c", *more_tik);
	
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

int atoi(const char* s)
{
	long int v=0;
	int sign=1;
	while ( *s == ' '  ||  (unsigned int)(*s - 9) < 5u) s++;
	switch (*s)
	{
	case '-':
		sign=-1;
	case '+':
		++s;
	}
	while ((unsigned int) (*s - '0') < 10u)
	{
		v=v*10+*s-'0';
		++s;
	}
	return sign==-1?-v:v;
}


//============下面是乱七八糟的字符串处理函数=================
char * strpbrk(const char *s1, const char *s2)
{
	for (; *s1; s1++) {
		if (strchr (s2, *s1) != 0)
			return ((char *)s1);
	}
	return (0);
}

char *strindex(const char *s, int c)
{
	if (s) {
		for (; *s; s++) {
			if (*s == c)
				return ((char *)s);
		}
	}
	return (0);
}
const short _C_tolower_[1 + 256] = {
	EOF,
	0x00,	0x01,	0x02,	0x03,	0x04,	0x05,	0x06,	0x07,
	0x08,	0x09,	0x0a,	0x0b,	0x0c,	0x0d,	0x0e,	0x0f,
	0x10,	0x11,	0x12,	0x13,	0x14,	0x15,	0x16,	0x17,
	0x18,	0x19,	0x1a,	0x1b,	0x1c,	0x1d,	0x1e,	0x1f,
	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,	0x27,
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
	0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
	0x38,	0x39,	0x3a,	0x3b,	0x3c,	0x3d,	0x3e,	0x3f,
	0x40,	'a',	'b',	'c',	'd',	'e',	'f',	'g',
	'h',	'i',	'j',	'k',	'l',	'm',	'n',	'o',
	'p',	'q',	'r',	's',	't',	'u',	'v',	'w',
	'x',	'y',	'z',	0x5b,	0x5c,	0x5d,	0x5e,	0x5f,
	0x60,	0x61,	0x62,	0x63,	0x64,	0x65,	0x66,	0x67,
	0x68,	0x69,	0x6a,	0x6b,	0x6c,	0x6d,	0x6e,	0x6f,
	0x70,	0x71,	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,
	0x78,	0x79,	0x7a,	0x7b,	0x7c,	0x7d,	0x7e,	0x7f,
	0x80,	0x81,	0x82,	0x83,	0x84,	0x85,	0x86,	0x87,
	0x88,	0x89,	0x8a,	0x8b,	0x8c,	0x8d,	0x8e,	0x8f,
	0x90,	0x91,	0x92,	0x93,	0x94,	0x95,	0x96,	0x97,
	0x98,	0x99,	0x9a,	0x9b,	0x9c,	0x9d,	0x9e,	0x9f,
	0xa0,	0xa1,	0xa2,	0xa3,	0xa4,	0xa5,	0xa6,	0xa7,
	0xa8,	0xa9,	0xaa,	0xab,	0xac,	0xad,	0xae,	0xaf,
	0xb0,	0xb1,	0xb2,	0xb3,	0xb4,	0xb5,	0xb6,	0xb7,
	0xb8,	0xb9,	0xba,	0xbb,	0xbc,	0xbd,	0xbe,	0xbf,
	0xc0,	0xc1,	0xc2,	0xc3,	0xc4,	0xc5,	0xc6,	0xc7,
	0xc8,	0xc9,	0xca,	0xcb,	0xcc,	0xcd,	0xce,	0xcf,
	0xd0,	0xd1,	0xd2,	0xd3,	0xd4,	0xd5,	0xd6,	0xd7,
	0xd8,	0xd9,	0xda,	0xdb,	0xdc,	0xdd,	0xde,	0xdf,
	0xe0,	0xe1,	0xe2,	0xe3,	0xe4,	0xe5,	0xe6,	0xe7,
	0xe8,	0xe9,	0xea,	0xeb,	0xec,	0xed,	0xee,	0xef,
	0xf0,	0xf1,	0xf2,	0xf3,	0xf4,	0xf5,	0xf6,	0xf7,
	0xf8,	0xf9,	0xfa,	0xfb,	0xfc,	0xfd,	0xfe,	0xff
};

const short *_tolower_tab_ = _C_tolower_;

int strcasecmp(const char *s1, const char *s2)
{
	const short *cm = _tolower_tab_ + 1;
	const unsigned char *us1 = (const unsigned char *)s1;
	const unsigned char *us2 = (const unsigned char *)s2;

	while (cm[*us1] == cm[*us2++])
		if (*us1++ == '\0')
			return (0);
	return (cm[*us1] - cm[*--us2]);
}

int strncmp(const char *cs,const char *ct, unsigned int count)
{
	register signed char __res = 0;

	while (count) 
	{
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

int striequ(const char *s1, const char *s2)//完全相等返回1
{
	if(strlen(s1)==strlen(s2)&&strcmp(s1, s2)==0)
		return 1;
	return 0;
}


int strbequ(const char *s1, const char *s2)
{

	if (!s1 || !s2)
		return (0);
	for (; *s1 && *s2; s1++, s2++)
		if (*s1 != *s2)
			return (0);
	if (!*s2)
		return (1);
	return (0);
}

char *strposn(const char *p, const char *q)
{
	const char *s, *t;

	if (!p || !q)
		return (0);

	if (!*q)
		return ((char *)(p + strlen (p)));
	for (; *p; p++) 
	{
		if (*p == *q) 
		{
			t = p;
			s = q;
			for (; *t; s++, t++) 
			{
				if (*t != *s)
					break;
			}
			if (!*s)
				return ((char *)p);
		}
	}
	return (0);
}


#define BANCHOR		(0x80|'^')
#define EANCHOR		(0x80|'$')

/** int strpat(p,pat) return 1 if pat matches p, else 0; wildcards * and ? */
int strpat(const char *s1, const char *s2)
{
	char *p, *pat;
	char *t, tmp[MAXLN];
	char src1[MAXLN], src2[MAXLN];

	if (!s1 || !s2)
		return (0);

	p = src1;
	pat = src2;
	*p++ = BANCHOR;
	while (*s1)
		*p++ = *s1++;
	*p++ = EANCHOR;
	*p = 0;
	*pat++ = BANCHOR;
	while (*s2)
		*pat++ = *s2++;
	*pat++ = EANCHOR;
	*pat = 0;

	p = src1;
	pat = src2;
	for (; *p && *pat;) 
	{
		if (*pat == '*') {
			pat++;
			for (t = pat; *t && *t != '*' && *t != '?'; t++);
			strncpy (tmp, pat, t - pat);
			tmp[t - pat] = '\0';
			pat = t;
			t = strposn (p, tmp);
			if (t == 0)
				return (0);
			p = t + strlen (tmp);
		}
		else if (*pat == '?' || *pat == *p) {
			pat++;
			p++;
		}
		else
			return (0);
	}
	if (!*p && !*pat)
		return (1);
	if (!*p && *pat == '*' && !*(pat + 1))
		return (1);
	return (0);
}

#define ULONG_MAX 	4294967295UL
#define LONG_MAX 	2147483647L
#define LONG_MIN 	(-LONG_MAX-1)
#define	isspace(c)	((_ctype_ + 1)[(unsigned char)(c)] & _S)

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    int c;
    unsigned long result = 0L;
    unsigned long limit;
    int negative = 0;
    int overflow = 0;
    int digit;

    while ((c = *nptr) && isspace(c)) /* skip leading white space */
      nptr++;

    if ((c = *nptr) == '+' || c == '-') 
	{ /* handle signs */
		negative = (c == '-');
		nptr++;
    }

    if (base == 0) 
	{		/* determine base if unknown */
		base = 10;
		if (*nptr == '0') 
		{
		    base = 8;
		    nptr++;
		    if ((c = *nptr) == 'x' || c == 'X') 
			{
				base = 16;
				nptr++;
		    }
		}
    } 
	else if (base == 16 && *nptr == '0') 
	{	
		/* discard 0x/0X prefix if hex */
		nptr++;
		if ((c = *nptr == 'x') || c == 'X')
		  nptr++;
    }

    limit = ULONG_MAX / base;	/* ensure no overflow */

    nptr--;			/* convert the number */
    while ((c = *++nptr) != 0) 
	{
		if (isdigit(c))
		  digit = c - '0';
		else if(isalpha(c))
		  digit = c - (isupper(c) ? 'A' : 'a') + 10;
		else
		  break;
		if (digit < 0 || digit >= base)
		  break;
		if (result > limit)
		  overflow = 1;
		if (!overflow) 
		{
		    result = base * result;
		    if (digit > ULONG_MAX - result)
		      overflow = 1;
		    else	
		      result += digit;
		}
	}
	
    if (negative && !overflow)	/* BIZARRE, but ANSI says we should do this! */
      result = 0L - result;
    if (overflow) 
	{
		extern int errno;
		errno = ERANGE;
		result = ULONG_MAX;
    }

    if (endptr != NULL)		/* point at tail */
      *endptr = (char *)nptr;
    return result;
}




