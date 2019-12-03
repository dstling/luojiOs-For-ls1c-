
#include "queue.h"

#define letoh16(x) (x)
#define letoh32(x) (x)
#define min(a,b) (((a) < (b)) ? (a) : (b))

#define	FS_NULL		0	/* Null FileSystem */
#define	FS_TTY		1	/* tty://	TTY */
#define	FS_NET		2	/* tftp:// 	Network */
#define	FS_SOCK		3	/* socket://	Socket */
#define	FS_DEV		4	/* dev://	Raw device */
#define	FS_MEM		5	/* mem://	Memory */
#define	FS_FILE		6	/* file://	Structured File */

#define	OPEN_MAX	10	/* max open files per process */

#define	 ENOENT		2	/* No such file or directory */
#define  EIO		5	/* I/O error */
#define  ENXIO		6	/* No such device or address */
#define  EBADF		9  /* Bad file number */
#define  EAGAIN		11  /* Try again */
#define  ENOMEM		12  /* Out of memory */
#define  EFAULT		14  /* Bad address */
#define  ENODEV		19  /* No such device */
#define  EISDIR		21  /* Is a directory */
#define  EINVAL		22  /* Invalid argument */
#define  EMFILE		24  /* Too many open files */
#define  ENOSPC		28  /* No space left on device */
#define  ESPIPE       29  /* Illegal seek */
#define  EROFS		30  /* Read-only file system */
#define  ERANGE		34  /* Math result not representable */

struct FileSystem{
	char	*devname;
	int	fstype;
	int	(*open) (int, const char *, int, int);
	int	(*read) (int , void *, unsigned int);
	int	(*write) (int , const void *, unsigned int);
	off_t	(*lseek) (int , off_t, int);
	int	(*close) (int);
	int	(*ioctl) (int , unsigned long , ...);
	SLIST_ENTRY(FileSystem)	i_next;
};
typedef struct FileSystem FileSystem;

struct File {
	short valid;
	off_t posn;
	FileSystem *fs;
	void  *data;
};
typedef struct File File;

extern File _file[] ;
extern int errno;

#define	SEEK_SET	0	/* set file offset to offset */
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#define	SEEK_END	2	/* set file offset to EOF plus offset */

#define	O_RDONLY	0x0000		/* open for reading only */
#define	O_WRONLY	0x0001		/* open for writing only */
#define	O_RDWR		0x0002		/* open for reading and writing */
#define	O_ACCMODE	0x0003		/* mask for above modes */

#define	O_NONBLOCK	0x0004		/* no delay */

int open(const char *filename,int mode);//mode=0 read mode=1 write mode=2 read and write
int close(int fd);
int read (int fd,void *buf,unsigned int n);
int write (int fd, const void *buf, size_t n);
off_t lseek(int fd, off_t offset, int whence);
int ioctl(int fd, unsigned long op, ...);







struct tftpFileStru{
	char tftpOpenAddr[128];
	unsigned int openMode;
	char * recvBuf;
	unsigned int recvLen;
};


