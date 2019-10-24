#include <types.h>

#include <lwip/sockets.h>
#include <lwip/netdb.h>

#include "queue.h"

#include "netio.h"
#include "filesys.h"
#define DebugPR printf("Debug info %s %s:%d\n",__FILE__ , __func__, __LINE__)



#define	EUNDEF		0		/* not defined */

#define	RRQ		01			/* read request */
#define	WRQ		02			/* write request */
#define	DATA	03			/* data packet */
#define	ACK		04			/* acknowledgement */
#define	ERROR	05			/* error code */

#define	SEEK_SET	0	/* set file offset to offset */
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#define	SEEK_END	2	/* set file offset to EOF plus offset */

#define	TIMEOUT		100000		/* secs between rexmts */
#define MAXREXMT	0x7fffffff		/* no of rexmts */

static const int tftperrmap[] = {
    EIO,	/* EUNDEF	not defined */
    ENOENT,	/* ENOTFOUND	file not found */
    EACCES,	/* EACCESS	access violation */
    ENOSPC,	/* ENOSPACE	disk full or allocation exceeded */
    EIO,	/* EBADOP	illegal TFTP operation */
    EINVAL,	/* EBADID	unknown transfer ID */
    EEXIST,	/* EEXISTS	file already exists */
    EACCES	/* ENOUSER	no such user */
};



static const struct servtab servtab[] = {
    {"tftp",	0,	69,	UDP},
    {0}
};

static int servidx;
int _serv_stayopen;

void setservent(int x)
{
	servidx = 0;
}

void endservent()
{
	servidx = -1;
}    


struct servent *getservent(void)
{
	static struct servent sv;
	static char *aliases[2];
	const struct servtab *st;

	if (servidx < 0)
	      return (0);

	st = &servtab[servidx];
	if (!st->name)
		return (0);

	sv.s_name = (char *)st->name;
	aliases[0] = (char *)st->alias; aliases[1] = 0;
	sv.s_aliases = aliases;
	sv.s_port = htons(st->port);
	sv.s_proto = (st->proto == TCP) ? "tcp" : "udp";

	servidx++;
	return (&sv);
}


struct servent * getservbyname(const char *name, const char *proto)
{
	struct servent *p;
	char **cp;

	setservent(_serv_stayopen);
	while ((p = getservent()) != 0)
	{
		if (strcmp(name, p->s_name) == 0)
			goto gotname;
		for (cp = p->s_aliases; *cp; cp++)
			if (strcmp(name, *cp) == 0)
				goto gotname;
		continue;
gotname:
		if (proto == 0 || strcmp(p->s_proto, proto) == 0)
			break;
	}
	if (!_serv_stayopen)
		endservent();
	return (p);
}



static void myifup()
{

}

static int makerequest(int request, const char *name, struct tftphdr *tp, const char *mode)
{
	register char *cp;

	tp->th_opcode = htons((u_short)request);
	cp = tp->th_stuff;
	strcpy(cp, name);
	cp += strlen(name);
	*cp++ = '\0';
	strcpy(cp, mode);
	cp += strlen(mode);
	*cp++ = '\0';
	return (cp - (char *)tp);
}

static void tftpnxtblk (struct tftpfile *tfp)
{
	tfp->block++;
	if (tfp->flags & O_NONBLOCK)
		dotik (20000, 0);
}

static int synchnet(int	f)/* socket to flush */
{
	int i, j = 0;
	static char rbuf[PKTSIZE];
	struct sockaddr_in from;
	int fromlen;
	while (1) 
	{
		(void) ioctl(f, FIONREAD, &i);
		if (i) 
		{
			j++;
			fromlen = sizeof from;
			(void) recvfrom(f, rbuf, sizeof (rbuf), 0,(struct sockaddr *)&from, &fromlen);
		} 
		else 
		{
			return(j);
		}
	}
}

static int tftprrq (struct tftpfile *tfp,struct tftphdr * req,int size)
{
	struct tftphdr *rp;
	struct sockaddr_in from;
	fd_set ifds;
	int fromlen, n;
	struct timeval timo;
	int rexmt = 0;

	while (1) 
	{
#ifdef TFTPDBG
		if (tftptrace)
			tpacket("sent", req, size);
#endif
		if (sendto(tfp->sock, req, size, 0, (struct sockaddr *)&tfp->sin,sizeof (tfp->sin)) != size) 
		{
			printf("tftp: sendto\n");
			return (-1);
		}

		if (tfp->eof)
			/* reached eof, no more to read */
			return 0;

		FD_ZERO(&ifds);
		FD_SET(tfp->sock, &ifds);
		timo.tv_sec = 0; 
		timo.tv_usec = TIMEOUT;
		int ans=select (tfp->sock + 1, &ifds, 0, 0, &timo);
		//printf("tftprrq ans:%d\n",ans);
		switch (ans) 
		{
			case -1:
				printf("tftp: select\n");
				return (-1);
			case 0:
			if(rexmt==(MAXREXMT>>1))
				myifup();
			if (++rexmt > MAXREXMT) 
			{
				errno = ETIMEDOUT;
				return (-1);
			}
			printf ("tftp: timeout, retry %d\n", rexmt);//这里会超时卡住
			//rt_thread_sleep(100);
			if(rexmt>5)//超时5次返回-2
				return -2;
			continue;
		}

		fromlen = sizeof (from);
		rp = (struct tftphdr *) tfp->buf;
		n = recvfrom(tfp->sock, rp, PKTSIZE, 0,(struct sockaddr *)&from, &fromlen);
		//printf("tftprrq recvfrom n:%d\n",n);
		if (n < 0) 
		{
			printf("tftp: recvfrom");
			return (-1);
		}
#ifdef TFTPDBG
		if (tftptrace)
			tpacket("received", rp, n);
#endif

		if (tfp->block <= 1)
			tfp->sin.sin_port = from.sin_port;
		else if (from.sin_port != tfp->sin.sin_port)
			continue;
		/* should verify client address completely? */

		rp->th_opcode = ntohs(rp->th_opcode);
		rp->th_block = ntohs(rp->th_block);
		if (rp->th_opcode == ERROR) 
		{
			if (rp->th_msg[0])
				printf("tftp: %s\n", rp->th_msg);
			errno = tftperrmap[rp->th_code & 7];
			return (-1);
		}

		if (rp->th_opcode == DATA) 
		{
			int j;
			if (rp->th_block == tfp->block) 
			{
				/* got the packet */
				n -= 4;
				if (n < SEGSIZE)
					tfp->eof = 1;
				return (n);
			}

			/* On an error, try to synchronize
			 * both sides.
			 */
			j = synchnet(tfp->sock);
			if (j)
				printf ("tftp: discarded %d packets\n", j);

			if (rp->th_block != tfp->block - 1)
				return (-1);
		}
	}
}



static int  tftpwrq (struct tftpfile *tfp,struct tftphdr * req,int size)
{
	static char ackbuf[PKTSIZE];
	struct tftphdr *rp;
	struct sockaddr_in from;
	fd_set ifds;
	int fromlen, n;
	struct timeval timo;
	int rexmt = 0;

	while (1) {
#ifdef TFTPDBG
		if (tftptrace)
			tpacket("sent", req, size);
#endif
		if (sendto(tfp->sock, req, size+4, 0, (struct sockaddr *)&tfp->sin,sizeof (tfp->sin)) != size+4) 
		{
			printf("tftp: sendto\n");
			return (-1);
		}

		FD_ZERO(&ifds);
		FD_SET(tfp->sock, &ifds);
		timo.tv_sec = 0;
		timo.tv_usec = TIMEOUT;
		switch (select (tfp->sock + 1, &ifds, 0, 0, &timo)) 
		{
			case -1:
				printf("tftp: select\n");
				return (-1);
			case 0:
				if(rexmt==(MAXREXMT>>1))
					myifup();
				if (++rexmt > MAXREXMT) 
				{
					errno = ETIMEDOUT;
					return (-1);
				}
				printf ("tftp: timeout, retry %d\n", rexmt);
				continue;
		}

		fromlen = sizeof (from);
		rp = (struct tftphdr *) ackbuf;
		n = recvfrom(tfp->sock, rp, PKTSIZE, 0,(struct sockaddr *)&from, &fromlen);
		if (n < 0) 
		{
			printf("tftp: recvfrom\n");
			return (-1);
		}
#ifdef TFTPDBG
		if (tftptrace)
			tpacket("received", rp, n);
#endif

		if (tfp->block == 0)
			tfp->sin.sin_port = from.sin_port;
		else if (from.sin_port != tfp->sin.sin_port)
			continue;
		/* should verify client address completely? */

		rp->th_opcode = ntohs(rp->th_opcode);
		rp->th_block = ntohs(rp->th_block);
		if (rp->th_opcode == ERROR) 
		{
			if (rp->th_msg[0])
				printf( "tftp: %s\n", rp->th_msg);
			errno = tftperrmap[rp->th_code & 7];
			return (-1);
		}

		if (rp->th_opcode == ACK) 
		{
			int j;
			if (rp->th_block == tfp->block)
				/* acknowledged packet */
				return (size);

			/* On an error, try to synchronize
			 * both sides.
			 */
			j = synchnet(tfp->sock);
			if (j)
				printf ( "tftp: discarded %d packets\n", j);

			if (rp->th_block != tfp->block - 1)
				return (-1);
		}
	}
}


static int tftpopen (int fd, struct Url *url, int flags, int perms)
{
	struct hostent *hp;
	struct servent *sp;
	const char *mode;
	char *host;
	static char hbuf[MAXHOSTNAMELEN];
	static char reqbuf[PKTSIZE];
	struct tftpfile *tfp;
	struct tftphdr *rp;
	int oflags, size;
	NetFile *nfp = (NetFile *)_file[fd].data;
	
	oflags = flags & O_ACCMODE; 
	if (oflags != O_RDONLY && oflags != O_WRONLY) 
	{
		errno = EACCES;
		return -1;
	}

	sp = getservbyname("tftp", "udp");
	if (!sp) 
	{
		errno = EPROTONOSUPPORT;
		return -1;
	}

#ifdef TFTPDBG
	tftptrace = getenv ("tftptrace") ? 1 : 0;
#endif

	if (strlen(url->hostname) != 0) 
	{
		host = url->hostname;
	} 
	else 
	{
		host = getenv ("tftphost");
		if (!host) 
		{
			printf ("tftp: missing/bad host name: %s\n", url->filename);
			errno = EDESTADDRREQ;
			return -1;
		}
	}

	tfp = (struct tftpfile *) malloc (sizeof (struct tftpfile));
	if (!tfp) 
	{
		errno = ENOBUFS;
		return -1;
	}
	memset(tfp,0,sizeof (struct tftpfile));
	nfp->data = (void *)tfp;
	
	tfp->sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (tfp->sock < 0)
		goto error;

	tfp->sin.sin_family = AF_INET;
	if (bind(tfp->sock, (struct sockaddr *)&tfp->sin, sizeof (tfp->sin)) < 0)
		goto error;
	printf("host:%s\n",host);
	hp = gethostbyname(host);
	if (hp) 
	{
		tfp->sin.sin_family = (unsigned char)hp->h_addrtype;
		memcpy((void *)&tfp->sin.sin_addr,hp->h_addr, hp->h_length);
		strncpy (hbuf, hp->h_name, sizeof(hbuf) - 1);
		hbuf[sizeof(hbuf)-1] = '\0';
		host = hbuf;
	} 
	else 
	{
		tfp->sin.sin_family = AF_INET;
		tfp->sin.sin_addr.s_addr = inet_addr (host);
		if (tfp->sin.sin_addr.s_addr == -1) 
		{
			printf ("tftp: bad internet address: %s\n", host);
			errno = EADDRNOTAVAIL;
			goto error;
		}
	}
	tfp->sin.sin_port = sp->s_port;
	
	rp = (struct tftphdr *)reqbuf;
	tfp->flags = flags;
	mode = "octet";

	if (oflags == O_RDONLY) 
	{
		tfp->block = 1;
		size = makerequest(RRQ, url->filename, rp, mode);
		size = tftprrq (tfp, rp, size);
		tfp->end = tfp->start + size;
		printf("tftp open,size:%d\n",size);
	} 
	else 
	{
		tfp->block = 0;
		size = makerequest(WRQ, url->filename, rp, mode);
		size = tftpwrq (tfp, rp, size - 4);
	}
	
	if (size >= 0)
		return 0;


error:
	if (tfp->sock >= 0)
		lwip_close (tfp->sock);
	free (tfp);
	return -1;
}

static int tftpread (int fd,void * buf,int nread)
{
	NetFile	*nfp;
	struct tftpfile *tfp;
	struct tftphdr *rp;
	int nb, n;

	nfp = (NetFile *)_file[fd].data;
	tfp = (struct tftpfile *)nfp->data;
    
	if ((tfp->flags & O_ACCMODE) != O_RDONLY) 
	{
		errno = EPERM;
		return (-1);
	}

	rp = (struct tftphdr *) tfp->buf;

	/* continue while more bytes wanted, and more available */
	for (nb = nread; nb != 0 && tfp->start < tfp->end; ) 
	{
		if (tfp->foffs >= tfp->start && tfp->foffs < tfp->end) 
		{
			/* got some data that's in range */
			n = tfp->end - tfp->foffs;
			if (n > nb) n = nb;
			memcpy ( buf,&rp->th_data[tfp->foffs - tfp->start], n);
			tfp->foffs += n;
			buf += n;
			nb -= n;
		} 

		if (tfp->foffs >= tfp->end) 
		{
			/* buffer is empty, ack last packet and refill */
			struct tftphdr ack;
			ack.th_opcode = htons((u_short)ACK);
			ack.th_block = htons((u_short)tfp->block);
			tftpnxtblk (tfp);
			n = tftprrq (tfp, &ack, 4);
			if (n < 0)
			{
				printf("%s,%s,%d,tftpread error\n",__FILE__ , __func__, __LINE__);
				return (-1);
			}
			tfp->start = tfp->end;
			tfp->end   = tfp->start + n;
		}
	}
	
	return nread - nb;
}

static int tftpwrite (int fd,const void * buf,int nwrite)
{
	NetFile *nfp;
	struct tftpfile *tfp;
	struct tftphdr *dp;
	int nb, n;

	nfp = (NetFile *)_file[fd].data;
	tfp = (struct tftpfile *)nfp->data;

	if ((tfp->flags & O_ACCMODE) != O_WRONLY) 
	{
		errno = EPERM;
		return (-1);
	}

	dp = (struct tftphdr *)tfp->buf;
	for (nb = nwrite; nb != 0; ) 
	{
		n = SEGSIZE - tfp->end;
		if (n > nb) n = nb;
		memcpy (&dp->th_data[tfp->end],buf,  n);
		tfp->end += n;
		tfp->foffs += n;
		buf += n;
		nb -= n;

		if (tfp->end == SEGSIZE) 
		{
			/* buffer is full, send it */
			tftpnxtblk (tfp);
			dp->th_opcode = htons((u_short)DATA);
			dp->th_block = htons((u_short)tfp->block);
			n = tftpwrq (tfp, dp, tfp->end);
			if (n < 0)
				return (-1);
			tfp->end = 0;
		}
	}
	return nwrite - nb;
}

static long tftplseek (int fd,long offs,int how)
{
	NetFile *nfp;
	struct tftpfile *tfp;
	long noffs;

	nfp = (NetFile *)_file[fd].data;
	tfp = (struct tftpfile *)nfp->data;

	switch (how) 
	{
		case SEEK_SET:
			noffs = offs; 
			break;
		case SEEK_CUR:
			noffs = tfp->foffs + offs; 
			break;
		case SEEK_END:
		default:
			return -1;
	}
	
	if ((tfp->flags & O_ACCMODE) == O_WRONLY) 
	{
		if (noffs != tfp->foffs) 
		{
			errno = ESPIPE;
			return (-1);
		}
	} 
	else 
	{
		if (noffs < tfp->start) 
		{
			errno = ESPIPE;
			return (-1);
		}
		tfp->foffs = noffs;
	}
	
	return (tfp->foffs);
}

static int tftpioctl (int fd,int op,void * argp)
{
	int errno=-1;
	errno = ENOTTY;
	return -1;
}

static int tftpclose (int fd)
{
	NetFile *nfp;
	struct tftpfile *tfp;
	struct tftphdr *tp;

	nfp	= (NetFile *)_file[fd].data;
	tfp = (struct tftpfile *)nfp->data;

	tp = (struct tftphdr *) tfp->buf;
	if ((tfp->flags & O_ACCMODE) == O_WRONLY) 
	{
		int n = 0;
		/* flush last block */
		tftpnxtblk (tfp);
		tp->th_opcode = htons((u_short)DATA);
		tp->th_block = htons((u_short)tfp->block);
		n = tftpwrq (tfp, tp, tfp->end);
		if (n >= 0 && tfp->end == SEGSIZE) 
		{
			/* last block == SEGSIZE, so send empty one to terminate */
			tftpnxtblk (tfp);
			tp->th_opcode = htons((u_short)DATA);
			tp->th_block = htons((u_short)tfp->block);
			(void) tftpwrq (tfp, tp, 0);
		}
	} 
	else 
	{
		if (tfp->foffs < tfp->end || !tfp->eof) 
		{
			const char *msg;
			int length;
	    
			/* didn't reach eof, so send a nak to terminate */
			tp->th_opcode = htons((u_short)ERROR);
			tp->th_code = htons((u_short)EUNDEF);
			msg = "file closed";
			strcpy(tp->th_msg, msg);
			length = strlen(msg) + 4;
#ifdef TFTPDBG
			if (tftptrace)
				tpacket("sent", tp, length);
#endif
			if (sendto(tfp->sock, tp, length, 0, (struct sockaddr *)&tfp->sin,sizeof (tfp->sin)) != length)
				printf("sendto(eof)\n");
		}
	}
	lwip_close (tfp->sock);
	free (tfp);
	return (0);
}

///*
static NetFileOps tftpops = {
	"tftp",
	tftpopen,
	tftpread,
	tftpwrite,
	tftplseek,
	tftpioctl,
	tftpclose
};

//*/
void init_netfs_tftp()
{
	/*
	 * Install ram based file system.
	 */
	netfs_init(&tftpops);
}


