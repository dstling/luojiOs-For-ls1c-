#define	FS_NULL		0	/* Null FileSystem */
#define	FS_TTY		1	/* tty://	TTY */
#define	FS_NET		2	/* tftp:// 	Network */
#define	FS_SOCK		3	/* socket://	Socket */
#define	FS_DEV		4	/* dev://	Raw device */
#define	FS_MEM		5	/* mem://	Memory */
#define	FS_FILE		6	/* file://	Structured File */

#define	OPEN_MAX		   10	/* max open files per process */

#define	ENOENT		2		/* No such file or directory */
#define  EMFILE          24  /* Too many open files */

struct FileSystem{
	char	*devname;
	int	fstype;
	int	(*open) (int, const char *, int, int);
	int	(*read) (int , void *, unsigned int);
	int	(*write) (int , const void *, unsigned int);
	long	(*lseek) (int , long, int);
	int	(*close) (int);
	int	(*ioctl) (int , unsigned long , ...);
	SLIST_ENTRY(FileSystem)	i_next;
};
typedef struct FileSystem FileSystem;

struct File {
	short valid;
	long posn;
	FileSystem *fs;
	void  *data;
};
typedef struct File File;

extern File _file[] ;


#define BANCHOR		(0x80|'^')
#define EANCHOR		(0x80|'$')


#define	O_RDONLY	0x0000		/* open for reading only */
#define	O_WRONLY	0x0001		/* open for writing only */
#define	O_RDWR		0x0002		/* open for reading and writing */
#define	O_ACCMODE	0x0003		/* mask for above modes */

#define	O_NONBLOCK	0x0004		/* no delay */


struct tftpFileStru{
	char tftpOpenAddr[128];
	unsigned int openMode;
	char * recvBuf;
	unsigned int recvLen;
};


