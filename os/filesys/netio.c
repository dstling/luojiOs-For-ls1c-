
#include <stdio.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#include "queue.h"

#include "netio.h"

#include "filesys.h"

extern int errno;


SLIST_HEAD(NetFileSystems, NetFileOps) NetFileSystems = SLIST_HEAD_INITIALIZER(NetFileSystems);


/*
 * Parse an url like:
 * "tftp://192.168.4.5/pmon"
 * "ftp://patrik@192.168.4.5/pmon"
 * "ftp://patrik:deadbeaf@sirus.pmon200.org/pmon"
 */
int parseUrl(const char *path, struct Url *url)
{
	char *dname;
	char *buf;
	dname = (char *)path;

	/*
	 * Parse protocol field
	 */
	if ((buf = strchr (dname, ':')) != NULL) 
	{
		int len = buf - dname;
		if (len + 1 > PROTOCOLSIZE) 
		{
			return (-1);
		}
		strncpy (url->protocol, dname, len);
		url->protocol[len] = 0;
		dname = buf;
	} 
	else 
	{
		return(-1);
	}
	if (strncmp (dname, "://", 3) == 0) 
	{
		dname += 3;
	} 
	else 
	{
		return (-1);
	}

	/*
	 * Parse username field
	 */
	if ((buf = strchr (dname, '@')) != NULL) 
	{
		int passwdlen;
		int unamelen;
		char *buf2;
		/*
		 * Check if we also provide a password
		 */
		if ((buf2 = strchr (dname, ':')) != 0) 
		{
			passwdlen = buf2 - dname;
			if (passwdlen + 1 > PASSWORDSIZE)
			{
				return (-1);
			}
			memcpy(buf2,url->password, passwdlen);
			url->password[passwdlen] = 0;
		} 
		else 
		{
			url->password[0] = 0;
			passwdlen = 0;
		}

		unamelen = buf - dname - passwdlen;
		if (unamelen + 1 > USERNAMESIZE) 
		{
			return (-1);
		}
		memcpy(dname,url->username, unamelen);
		url->username[unamelen] = 0;
		if (passwdlen)
			dname += passwdlen + 1;
		dname += unamelen + 1;
	} 
	else 
	{
		url->username[0] = 0;
	}
	/*
	 * Parse hostname field
	 */
	url->port=0;
	if ((buf = strchr (dname, ':')) != NULL)
	{
		int len = buf - dname;
		if (len + 1 > HOSTNAMESIZE) 
		{
			return (-1);
		}
		strncpy (url->hostname, dname, len);
		url->hostname[len] = 0;
		dname = buf + 1;
		if ((buf = strchr (dname, '/')) != NULL) 
		{
			url->port=atoi(dname);//strtoul(dname,0,0);//端口号
			dname= buf + 1;
		}
		else 
			return -1;		
	}
	else if ((buf = strchr (dname, '/')) != NULL) 
	{
		int len = buf - dname;
		if (len + 1 > HOSTNAMESIZE) 
		{
			return (-1);
		}
		strncpy (url->hostname, dname, len);
		url->hostname[len] = 0;
		dname = buf + 1;
	} 

	/*
	 * Parse Filename field
	 */
	strncpy(url->filename, dname, FILENAMESIZE);
	
	return (0);
}

int netfs_init(NetFileOps *fs)
{
	SLIST_INSERT_HEAD(&NetFileSystems, fs, i_next);
	return(0);
}

int netopen (int fd, const char *path, int flags, int perms)
{
	NetFileOps *ops;
	struct	Url url;
	NetFile *nfp;
	char *dname = (char *)path;
	//printf("netopen dname:%s,fd:%d\n",dname,fd);
	if (strncmp (dname, "/dev/net/", 9) == 0) 
	{
		dname += 9;
	}

	if(parseUrl(dname, &url) == -1) 
	{
		printf("netipen error in parseUrl.\n");
		return(-1);
	}
	//printf("netopen url.protocol:%s,url.filename:%s,url.hostname:%s\n",url.protocol,url.filename,url.hostname);
	SLIST_FOREACH(ops, &NetFileSystems, i_next) 
	{

		if (strcmp (url.protocol, ops->protocol) == 0) 
		{
		      nfp = (NetFile *)malloc(sizeof(NetFile));
		      if (nfp == NULL) 
			  {
			    printf("Out of space for allocating a NetFile\n");
			    return(-1);
		      }
		      nfp->ops = ops;
			  //printf("netopen ops.protocol:%s.\n",ops->protocol);
		      _file[fd].data = (void *)nfp;
		      if((*(ops->fo_open)) (fd, &url, flags, perms) != 0) 
			  {
			    free(nfp);
			    return(-1);
		      }
		}
	}
	return (fd);
}

int netread (int fd, void *buf, size_t nb)
{
	NetFile *nfp;

	nfp = (NetFile *)_file[fd].data;

	if (nfp->ops->fo_read)
		return ((nfp->ops->fo_read) (fd, buf, nb));
	else {
		return (-1);
	}
}

int netwrite (int fd, const void *buf, size_t nb)
{
	NetFile *nfp;

	nfp = (NetFile *)_file[fd].data;

	if (nfp->ops->fo_write)
		return ((nfp->ops->fo_write) (fd, buf, nb));
	else
		return (-1);
}

off_t netlseek (int fd, off_t offs, int how)
{
	NetFile *nfp;

	nfp = (NetFile *)_file[fd].data;

	if (nfp->ops->fo_lseek)
		return ((nfp->ops->fo_lseek) (fd, offs, how));
	else
		return (-1);
}

int netioctl (int fd, unsigned long op, ...)
{
	NetFile *nfp;
	void *argp;
	va_list ap;

	va_start(ap, op);
	argp = va_arg(ap, void *);
	va_end(ap);

	nfp = (NetFile *)_file[fd].data;
	
	if (nfp->ops->fo_ioctl)
		return ((nfp->ops->fo_ioctl) (fd, op, argp));
	else
		return (-1);
}

int netclose (int fd)
{
	NetFile *nfp;

	nfp = (NetFile *)_file[fd].data;
	
	if (nfp->ops->fo_close)
		return ((nfp->ops->fo_close) (fd));
	else
		return (-1);
}


static FileSystem netfs =
{
	"net", FS_NET,
	netopen,
	netread,
	netwrite,
	netlseek,
	netclose,
	netioctl
};

void init_fs_netfs()
{
	filefs_init(&netfs);
}
