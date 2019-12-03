
#define  ENOMEM       12  /* Out of memory */
#define  EINVAL       22  /* Invalid argument */
#define  EISDIR       21  /* Is a directory */
#define  EROFS        30  /* Read-only file system */

#define	MAXPHYS		(32 * 1024)	/* max raw I/O transfer size */

#define BLOCK_DEV_USB_STOR_FLAG 	0
#define BLOCK_DEV_DISK_STOR_FLAG 	1
struct biodev {
	//int		dstling_flag;
	int		dev_type;//打开的设备类型 usb=0 disk=1等
	int		dev_index;//被打开设备的索引
	int		opened_flag;//成功打开置1
	//dev_t	devno;
	off_t	base;//这个是分区起始的偏移位置
	
	off_t	offs;
	off_t	end;//这个值应该是某个设备的分区的最后一个位置
	
	//int	*maptab;
};

/* bytes to disk blocks */
#define	DEV_BSHIFT	9					/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)	//512

#define	btodb(x)	((x) >> DEV_BSHIFT)
#define dbtob(x)	((x) << DEV_BSHIFT)

#define	howmany(x, y)	(((x)+((y)-1))/(y))

#define	B_READ		0x00100000	/* Read buffer. */
extern int errno;

struct devsw {
	char	*name;
	int	(*open)(dev_t, int flags, int mode, void *);
	int	(*read)(dev_t dev, void *uio, int flag);
	int	(*write)(dev_t dev, void *uio, int flag);
	int	(*close)(dev_t dev, int flag, int mode, void *);
};


