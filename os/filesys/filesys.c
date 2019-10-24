
#include <stdio.h>
#include <shell.h>
#include <rtdef.h>
#include "queue.h"
#include "filesys.h"

#define DEBUG_FILESYS

#ifdef DEBUG_FILESYS
#define DebugPR printf("Debug info %s %s:%d\n",__FILE__ , __func__, __LINE__)
#define debug_filesys printf
#else
#define DebugPR
#define debug_filesys
#endif

int errno;

File _file[OPEN_MAX] =
{
	{0} 	// stdin 
	//{1}, 	// stdout 
	//{1},  	// stderr 
	//{1},  	// kbdin 
	//{1}  	// vgaout 
};

SLIST_HEAD(FileSystems, FileSystem) FileSystems = SLIST_HEAD_INITIALIZER(FileSystems);

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

//dname mtd0/xx  or fs/yaffs2@mtd1/xx
static int __try_open(const char *fname, int mode, char *dname, int lu, int fstype)
{
	FileSystem *fsp;

	SLIST_FOREACH(fsp, &FileSystems, i_next)
	{
		//debug_filesys("fname:%s dname:%s fsp->devname:%s\n",fname,dname,fsp->devname);
		//比对完后面这个字符串 如果相同则返回1 否则返回0
		if (dname && strbequ(dname, fsp->devname)) //dname=:fs/yaffs2@mtd1/home/test5.txt  fsp->devname=fs/yaffs2
		{
			if(fsp->open)
			{ 
				//第一次 fname=:/dev/fs/yaffs2@mtd1/home/test5.txt
				//这里调用yaffs2自己的open了
				//第二次  fname:mtd1/ dname:mtd1/ fsp->devname:mtd
				int xx=(*fsp->open)(lu, fname, mode, 0);//查找mtd分区，同时赋值mtdfile给  _file[lu].data mtdfile_open fname=mtd1/
				if(xx!=lu)
					return -1;	/* Error? */
			}
			break;
		}
		else if (fstype != FS_NULL && fsp->fstype == fstype) 
		{
			if(fsp->open && (*fsp->open)(lu, fname, mode, 0) == lu)
			{
				break;
			}
		}
	}
	
	if (fsp) 
	{
		/* Opened */
		_file[lu].valid = 1;
		_file[lu].posn = 0;
		_file[lu].fs = fsp;
		return(lu);
	}
	else 
	{
		errno = ENOENT;
		return -1;
	}
}

int open(const char *filename,int mode)//mode=0 read mode=1 write mode=2 read and write
{
	char 	*fname;
	char    *dname;
	int     lu, i;
	int 	fnamelen;
	
	fnamelen = strlen(filename);
	fname = (char *)malloc(fnamelen+1);
	memcpy(fname, filename, fnamelen+1);
	debug_filesys("open :%s\n",filename);
	for (lu = 0; lu < OPEN_MAX && _file[lu].valid; lu++)
	{
		debug_filesys("lu:%d valid:%d\n",lu,_file[lu].valid);
	}

	if (lu == OPEN_MAX) 
	{
		errno = EMFILE;
		free(fname,0);
		return -1;
	}
	_file[lu].valid = 1;

	dname = (char *)fname;
	if (strncmp (dname, "/dev/", 5) == 0) 
	{
		dname += 5;
		i = __try_open(fname, mode, dname, lu, 0);
		free(fname,0);
		if(i==-1)
		{
			_file[lu].valid = 0;
		}
		return i;
#ifdef never
		if (i < 0) 
		{
			dname = "disk";
			i = __try_open(fname, mode, dname, lu, 0);
		}
		if (i < 0) 
		{
			i = __try_open(fname, mode, NULL, lu, FS_FILE);
		}
#endif
	}
	
	else if (strpat(dname, "*ftp://*")) //tftp打开到这里
	{
		i = __try_open(fname, mode, "net", lu, 0);
		
	}
	else if (strpat(dname, "file://*")) {
		dname += 6;
		i = __try_open(dname, mode, NULL, lu, FS_FILE);
	}
	else if(strpat(dname, "http://*"))
	{
		i = __try_open(dname, mode, "net", lu, 0);
	}
    else if(strpat(dname, "nfs://*"))
    {
        i = __try_open(dname, mode, "net", lu, 0);
    }
	
	else 
	{
		i = __try_open(fname, mode, dname, lu, 0);
	}
	free(fname,0);
	
	if(i==-1)
	{
		_file[lu].valid = 0;
	}
	return i;
}

int close(int fd)
{
	 if ((fd < OPEN_MAX) && _file[fd].valid) {
		 _file[fd].valid = 0;
		 if (_file[fd].fs->close)
			 return (((_file[fd]).fs->close) (fd));
		 else
			 return (-1);
	 }
	 return (0);
}

int read (int fd,void *buf,unsigned int n)
     
{
	if ((fd < OPEN_MAX) && _file[fd].valid )
		if (_file[fd].fs->read)
			return (((_file[fd]).fs->read)(fd, (char *)buf, n));
		else
			return (-1);
	else
		return (-1);
}
long lseek(int fd, long offset, int whence)
{
	if ((fd < OPEN_MAX) && _file[fd].valid )
		if (_file[fd].fs->lseek)
			return (((_file[fd]).fs->lseek) (fd, offset, whence));
		else
			return (-1);
	else
		return (-1);
}

int ioctl(int fd, unsigned long op, ...)
{
	void *argp;
	va_list ap;
	
	va_start(ap, op);
	argp = va_arg(ap, void *);
	va_end(ap);
	
	if ((fd < OPEN_MAX) && _file[fd].valid )
		if (_file[fd].fs->ioctl)
			return (((_file[fd]).fs->ioctl) (fd, op, argp));
		else
			return (-1);
	else
		return(-1);
}

int filefs_init(FileSystem *fs)
{
	SLIST_INSERT_HEAD(&FileSystems, fs, i_next);
	return(0);
}






//=============================================================================

unsigned int tftpEveryReadSize=512;
int tftpBufSize=100*1024;
struct tftpFileStru tftpOpenfile;

void tftpOpenEntry(void*userdata)
{
	//register long temp = rt_hw_interrupt_disable();
	int fd=open(tftpOpenfile.tftpOpenAddr,tftpOpenfile.openMode|O_NONBLOCK);//mode  OpenLoongsonLib1c.bin
	printf("tftp open address:%s,mode:%d,fd:%d\n",tftpOpenfile.tftpOpenAddr,tftpOpenfile.openMode,fd);
	printf("loading...\n");
	if(fd>-1)
	{
		int retSize=0;
		
		char *bufPt=malloc(tftpBufSize);//存储获取的数据
		memset(bufPt,0,tftpBufSize);
		tftpOpenfile.recvBuf=bufPt;
		
		char* readTmp=malloc(tftpEveryReadSize);//每次读取用的缓存
		memset(readTmp,0,tftpEveryReadSize);
		
		while(1)
		{
			retSize=read(fd,readTmp,tftpEveryReadSize);
			if(retSize<1)//读完了
				break;
			
			tftpOpenfile.recvLen+=retSize;

			if(tftpOpenfile.recvLen>tftpBufSize)//空间不够了
			{
				tftpBufSize+=100*1024;//追加100k空间
				char *newMem=realloc(tftpOpenfile.recvBuf,tftpBufSize);
				tftpOpenfile.recvBuf=newMem;
				bufPt=newMem+tftpOpenfile.recvLen-retSize;
			}
			
			//printf("openTestEntry retSize:%d,readCounter:%d\n",retSize,tftpOpenfile.recvLen);
			//rt_thread_sleep(100);
			//show_hex(bufPt,retSize,0);
			
			memcpy(bufPt,readTmp,retSize);
			bufPt+=retSize;
		}
		close(fd);
		printf("\btftp transfer files successed!!!.\n");
		tgt_flashprogram(0xbfc00000, // address 
				tftpOpenfile.recvLen,	// size 
				(void *)tftpOpenfile.recvBuf,	// load 
				0);
		free(readTmp);
		free(tftpOpenfile.recvBuf);
	}
	else
		printf("tftpOpenEntry error.\n");
    //rt_hw_interrupt_enable(temp);
	THREAD_DEAD_EXIT;	
}

void tftpOpen(int argc,char**argv)
{
	if(argc!=3)
	{
		printf("useage: openTest filename mode(0,1,or 2) \n");
		return;
	}
	int mode=atoi(argv[2]);
	if(mode<0)
	{
		printf("mode errr,mode=0 read,mode=1,write,mode=2 read and write.\n");
		return;
	}
	memset(&tftpOpenfile,0,sizeof(tftpOpenfile));
	memcpy(tftpOpenfile.tftpOpenAddr,argv[1],strlen(argv[1]));
	tftpOpenfile.openMode=mode;
	
	thread_join_init("tftpThread",tftpOpenEntry,NULL,4092,16,50);
}
FINSH_FUNCTION_EXPORT(tftpOpen,tftpOpen tftp addr mode);

#include <env.h>
void updateLuojios(void)
{
	char *serverip=getenv(ENV_TFTP_SERVER_IP_NAME);
	if(serverip==0)
	{
		printf("no tftp server ip setting. please\"set %s ipaddr\"\n",ENV_TFTP_SERVER_IP_NAME);
		return;
	}
	char *pmonFileName=getenv(ENV_TFTP_PMON_FILE_NAME);
	if(pmonFileName==0)
	{
		printf("no pmon file name setting. please\"set %s filename\"\n",ENV_TFTP_PMON_FILE_NAME);
		return;
	}
	memset(&tftpOpenfile,0,sizeof(tftpOpenfile));
	char tftpHead[]="tftp://";
	memcpy(tftpOpenfile.tftpOpenAddr,tftpHead,strlen(tftpHead));
	strcat(tftpOpenfile.tftpOpenAddr,serverip);
	strcat(tftpOpenfile.tftpOpenAddr,"/");
	strcat(tftpOpenfile.tftpOpenAddr,pmonFileName);
	tftpOpenfile.openMode=0;
	thread_join_init("tftpThread",tftpOpenEntry,NULL,4092,16,50);
}
FINSH_FUNCTION_EXPORT(updateLuojios,flash new bootloader by tftp);










