
#ifndef __NETIO_H__
#define __NETIO_H__

struct	servent {
	char	*s_name;	/* official service name */
	char	**s_aliases;	/* alias list */
	int	s_port;		/* port # */
	char	*s_proto;	/* protocol to use */
};

#define MAXHOSTNAMELEN	256		/* max hostname size */
#define	SEGSIZE		512		/* data segment size */
#define PKTSIZE		(SEGSIZE+4)

struct tftpfile {
    struct sockaddr_in sin;
    u_short	block;
    short	flags;
    short	eof;
    int		sock;
    off_t 	start;//int
    off_t		end;//int
    off_t		foffs;//int
    char	buf[PKTSIZE];
};

struct	tftphdr {
	u_int16_t th_opcode;		/* packet type */
	union {
		u_int16_t tu_block;	/* block # */
		u_int16_t tu_code;	/* error code */
		char	tu_stuff[1];	/* request packet stuff */
	} th_u;
	char	th_data[1];		/* data or error string */
};
#define	th_block	th_u.tu_block
#define	th_code		th_u.tu_code
#define	th_stuff	th_u.tu_stuff
#define	th_msg		th_data

#define PROTOCOLSIZE	8
#define USERNAMESIZE	26
#define	PASSWORDSIZE	26
#define HOSTNAMESIZE	80
#define	FILENAMESIZE	80

struct Url {
	char		protocol[PROTOCOLSIZE];
	char		username[USERNAMESIZE];
	char		password[PASSWORDSIZE];
	char		hostname[HOSTNAMESIZE];
	char		filename[FILENAMESIZE];
	short		port;
};
typedef struct NetFileOps {
	char		*protocol;
	int		(*fo_open)	(int, struct Url *, int, int);
	int		(*fo_read)	(int, void *, int);
	int		(*fo_write)	(int, const void *, int);
	off_t	(*fo_lseek)	(int, off_t, int);
	int		(*fo_ioctl)	(int, int, void *);
	int		(*fo_close)	(int);
	SLIST_ENTRY(NetFileOps)	i_next;
	
} NetFileOps;

struct servtab {
    const char	*name;
    const char	*alias;
    short	port;
    short	proto;
};

#define TCP	0
#define UDP	1

typedef struct NetFile {
	NetFileOps	*ops;
	void		*data;
} NetFile;

extern int	netopen (int, const char *, int, int);
extern int	netread (int, void *, unsigned int);
extern int	netwrite (int, const void *, size_t);
extern 	off_t netlseek (int fd, off_t offs, int how);
extern int	netioctl (int, unsigned long, ...);
extern int	netclose (int);
extern int	netfs_init (NetFileOps *fs);

#endif /* __NETIO_H__ */
