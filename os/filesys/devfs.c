#include <stdio.h>
#include <types.h>
#include <shell.h>
#include <rtdef.h>
#include "filesys.h"
#include "devfs.h"

//#define DEBUG_DEVFS

#ifdef DEBUG_DEVFS
#define debug_devfs(format, arg...) printf("debug_devfs: " format, ## arg)
#else
#define debug_devfs(format, arg...)
#endif

//extern int errno;
static char _lbuff[DEV_BSIZE*2];
static char *lbuff;

struct biodev opendevs[OPEN_MAX];

int	usb_open(dev_t dev, int flag, int fmt, struct proc *p);
int	usb_close(dev_t dev, int flag, int fmt, struct proc *p);
int	usb_read(dev_t dev, struct uio *uio, int ioflag);
int	usb_write(dev_t dev, struct uio *uio, int ioflag);

struct devsw devswitch[] = {
	{ "usb", usb_open, usb_read, usb_write, usb_close},
	{ NULL},
};
	
static int read_device(int fd,void *buf,size_t blen)
{
	struct biodev *biodevOpened=&opendevs[fd];
	if(biodevOpened->opened_flag==1&&biodevOpened->dev_type==BLOCK_DEV_USB_STOR_FLAG)
		usb_read_dst(fd,buf,blen);
	return blen;
}

static int write_device(int fd,void *buf,size_t blen)
{
	printf("write_device#########################################################\n");
	return(blen);
}

/*
 *	Read data from a device.
 */

dev_t find_device(const char **name,struct biodev *biodevThis)
{
	dev_t dev;
	struct devsw *devsw;
	const char *pname;

	// Discard /dev/ and disk/ from name 
	pname = *name;
	if (strncmp(pname, "/dev/", 5) == 0)
		pname += 5;
	if (strncmp(pname, "disk/", 5) == 0) 
	{
		pname += 5;
	}

	if (*pname == '/')
		pname++;	// Skip over leading slash if dev is first 

	int retDev=find_usb_device(pname,biodevThis);
	if(retDev!=-1)
	{
		debug_devfs("open find_device:%s\n",pname);
		return 1;
	}
	//尝试打开其他设备

	
	return 0;
}

int devio_open(int fd, const char *name, int flags,int mode)//主要是为了初始化opendevs fd对应的值
{
	int mj;
	u_int32_t v;
	dev_t dev;
	struct biodev *biodevOpened;
	char strbuf[64], *strp, *p;

	biodevOpened = &opendevs[fd];
	if(biodevOpened->opened_flag==1)
		return 0;
	lbuff=(long)(_lbuff+DEV_BSIZE-1)&~(DEV_BSIZE-1);

	debug_devfs("1devio_open,name:%s\n",name);//1devio_open,name:/dev/usb0
	if(find_device(&name,biodevOpened))//找到设备了
	{
		biodevOpened->base = 0;
		biodevOpened->offs = 0;
		biodevOpened->end =0x10000000000;//0xffffffff; //
		biodevOpened->opened_flag=1;
		debug_devfs("2devio_open,name:%s,dev:%d\n",name,dev);//2devio_open,name:
		return 1;
	}
	
	return(0);
}

int devio_close(int fd)
{
	opendevs[fd].opened_flag=0;
}

int devio_read(int fd, void *buf,size_t blen)
{
	size_t leftlen;
	int ret=0;

	debug_devfs("0devio_read1,fd:%d,blen:%d,opendevs[fd].end:%ld,opendevs[fd].offs:%ld\n",fd,blen,opendevs[fd].end,opendevs[fd].offs);

/*

	if(opendevs[fd].offs < opendevs[fd].base ||opendevs[fd].offs > opendevs[fd].end) 
	{
		errno = EINVAL;
		return(-1);
	}
	
	if(blen > (opendevs[fd].end - opendevs[fd].offs))
	{
		blen = opendevs[fd].end - opendevs[fd].offs;
		debug_devfs("blen:%d,opendevs[fd].end:%ld,opendevs[fd].offs:%ld\n",blen,opendevs[fd].end,opendevs[fd].offs);
	}
*/
	if(opendevs[fd].offs < opendevs[fd].base) 
	{
		errno = EINVAL;
		return(-1);
	}

	if (blen == 0)
		return 0;

	leftlen=blen;
	debug_devfs("1.1devio_read1 blen:%d,leftlen:%d,opendevs[fd].end:%ld,opendevs[fd].offs:%ld\n",blen,leftlen,opendevs[fd].end,opendevs[fd].offs);

	while(leftlen)
	{
		if (opendevs[fd].offs & (DEV_BSIZE - 1) || leftlen < DEV_BSIZE) /*(long)buf&(DEV_BSIZE-1) ||*/ 
		{
			/* Check for unaligned read, eg we need to buffer */
			int suboffs = opendevs[fd].offs & (DEV_BSIZE - 1);
			int n=min(DEV_BSIZE-suboffs,leftlen);
			opendevs[fd].offs &= ~(DEV_BSIZE - 1);
			ret=read_device(fd, lbuff, DEV_BSIZE);
			if(ret<0)
				break;
			memcpy(buf,lbuff+suboffs,n);
			buf+=n;
			opendevs[fd].offs += suboffs+n;
			leftlen-=n;
		 }
		 else if(leftlen & (DEV_BSIZE - 1)) 
		 {
			int n=min(leftlen & ~(DEV_BSIZE - 1),MAXPHYS);
			ret = read_device(fd, buf, n);
			if (ret < 0)
				break;
			buf += n;
			opendevs[fd].offs += n;
			leftlen -= n;
		}
		else
		{
			int n=min(leftlen,MAXPHYS);
			ret = read_device(fd, buf, n);
#ifdef DEBUG_DEVFS
			//dataHexPrint2(buf,n,0,"devio_read buf");
#endif
			if(ret<0)
				break;
			buf += n;
			opendevs[fd].offs += n;
			debug_devfs("4devio_read leftlen:%d,n:%d,ret:%d，opendevs[fd].offs:%ld\n",leftlen,n,ret,opendevs[fd].offs);
			leftlen -= n;
		}
	}

	return ret<0?ret:blen-leftlen;
}

int devio_write(int fd, const void *buf,size_t blen)
{
	int mj;
	size_t leftlen;
	int ret;

	if (blen == 0)
		return(0);


	leftlen=blen;

	while(leftlen)
	{

	if (/*(long)buf&(DEV_BSIZE-1) ||*/ opendevs[fd].offs & (DEV_BSIZE - 1) || leftlen < DEV_BSIZE) {
	/* Check for unaligned read, eg we need to buffer */
		int suboffs = opendevs[fd].offs & (DEV_BSIZE - 1);
		int n=min(DEV_BSIZE-suboffs,leftlen);
		opendevs[fd].offs &= ~(DEV_BSIZE - 1);
		if(suboffs||n<DEV_BSIZE)
		{
		ret=read_device(fd, lbuff, DEV_BSIZE);
		 if(ret<0)break;
		}
		memcpy(lbuff+suboffs,buf,n);
		ret=write_device(fd, lbuff, DEV_BSIZE);
		if(ret<0)break;
		 buf+=n;
		 opendevs[fd].offs += suboffs+n;
		 leftlen-=n;
	 }
	 else if(leftlen & (DEV_BSIZE - 1)) {
		int n=min(leftlen & ~(DEV_BSIZE - 1),MAXPHYS);
		ret = write_device(fd, buf, n);
		if (ret < 0)
			break;
		buf += n;
		opendevs[fd].offs += n;
		leftlen -= n;
	}
	else
	{
		int n=min(leftlen,MAXPHYS);
		ret = write_device(fd, buf, n);
		if(ret<0)break;
		buf += n;
		opendevs[fd].offs += n;
		leftlen -= n;
	}
   }

	return ret<0?ret:blen-leftlen;
}

off_t devio_lseek(int fd,off_t offs,int whence)
{
	if(whence == 0)
		opendevs[fd].offs =opendevs[fd].base + offs;
	else
		opendevs[fd].offs += offs;

/*
	if(opendevs[fd].offs > opendevs[fd].end)
	{
		opendevs[fd].offs = opendevs[fd].end;
	}
*/	

	return	(opendevs[fd].offs - opendevs[fd].base);
}
static FileSystem diskfs = {
	"disk", FS_DEV,
	devio_open,
	devio_read,
	devio_write,
	devio_lseek,
	devio_close,
	NULL
};

void init_fs_disk()
{
	filefs_init(&diskfs);
}

