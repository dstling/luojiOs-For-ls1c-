
#ifndef _TYPES_H_
#define _TYPES_H_


//typedef unsigned long long u64;
//typedef unsigned int u32;
//typedef unsigned short u16;
//typedef unsigned char u8;
typedef signed long long s64;
//typedef signed long s32;
typedef signed short s16;
typedef signed char s8;
//typedef int bool;


typedef	signed char		        int8_t;
typedef	unsigned char		    u_int8_t;
//typedef	unsigned char		    uint8_t;
typedef	short			        int16_t;
typedef	unsigned short		    u_int16_t;
typedef	unsigned short		    uint16_t;
typedef	int			            int32_t;
typedef	unsigned int		    u_int32_t;
typedef	unsigned int		    uint32_t;
/* LONGLONG */
typedef	long long		        int64_t;
/* LONGLONG */
typedef	unsigned long long	    u_int64_t;
/* LONGLONG */
typedef	unsigned long long	    uint64_t;

typedef	u_int64_t	u_quad_t;	/* quads */
typedef	u_int64_t	quad_t;	/* quads */


typedef	long long off_t;		/* file offset */

typedef	int32_t		dev_t;		/* device number */

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;

typedef unsigned char	unchar;		/* Sys V compatibility */
typedef	unsigned short	ushort;		/* Sys V compatibility */
typedef	unsigned int	uint;		/* Sys V compatibility */
typedef unsigned long	ulong;		/* Sys V compatibility */
#endif

typedef unsigned int    size_t;

typedef  unsigned long pcitag_t;
typedef u_int32_t pcireg_t;		/* configuration space register XXX */

#endif /* _TYPES_H_ */

