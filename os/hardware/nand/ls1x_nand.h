#ifndef __LS1X_NAND_H_
#define __LS1X_NAND_H_

#define DATA_BUFF	0xa0000000
#define DMA_DESC	0xa0200000

#define DMA_ACCESS_ADDR	0x1fe78040	/* DMA对NAND操作的地址 */
#define ORDER_ADDR_IN	0xbfd01160	/* DMA配置寄存�?*/
#define DMA_DESC_NUM	64	/* DMA描述符占用的字节�?7x4 */
/* DMA描述�?*/
#define DMA_ORDERED		0x00
#define DMA_SADDR		0x04
#define DMA_DADDR		0x08
#define DMA_LENGTH		0x0c
#define DMA_STEP_LENGTH		0x10
#define DMA_STEP_TIMES		0x14
#define	DMA_CMD			0x18

/* registers and bit definitions */
#define LS1X_NAND_BASE	0xbfe78000
#define NAND_CMD		0x00	/* Control register */
#define NAND_ADDR_L		0x04	/* 读、写、擦除操作起始地址�?2�?*/
#define NAND_ADDR_H		0x08	/* 读、写、擦除操作起始地址�?�?*/
#define NAND_TIMING		0x0c
#define NAND_IDL		0x10	/* ID�?2�?*/
#define NAND_IDH		0x14	/* ID�?�?*/
#define NAND_STATUS		0x14	/* Status Register */
#define NAND_PARAM		0x18	/* 外部颗粒容量大小 */
#define NAND_OPNUM		0x1c	/* NAND读写操作Byte�?擦除为块�?*/
#define NAND_CS_RDY		0x20

/* NAND_TIMING寄存器定�?*/
#define HOLD_CYCLE	0x02
#define WAIT_CYCLE	0x0c

#define DMA_REQ			(0x1 << 25)
#define ECC_DMA_REQ		(0x1 << 24)
#define WAIT_RSRD_DONE	(0x1 << 14)
#define INT_EN			(0x1 << 13)
#define RS_WR			(0x1 << 12)
#define RS_RD			(0x1 << 11)
#define DONE			(0x1 << 10)
#define SPARE			(0x1 << 9)
#define MAIN			(0x1 << 8)
#define READ_STATUS		(0x1 << 7)
#define RESET			(0x1 << 6)
#define READ_ID			(0x1 << 5)
#define BLOCKS_ERASE	(0x1 << 4)
#define ERASE			(0x1 << 3)
#define WRITE			(0x1 << 2)
#define READ			(0x1 << 1)
#define COM_VALID		(0x1 << 0)

/* macros for registers read/write */
#define nand_writel(info, off, val)	\
	__raw_writel((val), (info)->mmio_base + (off))

#define nand_readl(info, off)		\
	__raw_readl((info)->mmio_base + (off))

//#define MAX_BUFF_SIZE	1048576	/* 1MByte 避免有时分配不成功？ */
#define MAX_BUFF_SIZE	10240
#define PAGE_SHIFT		12	/* 页内地址(列地址)A0-A11 */
#define LS1CSOC
#if defined(LS1ASOC) || defined(LS1CSOC)
	#define MAIN_ADDRH(x)		(x)
	#define MAIN_ADDRL(x)		((x) << PAGE_SHIFT)
	#define MAIN_SPARE_ADDRH(x)	(x)
	#define MAIN_SPARE_ADDRL(x)	((x) << PAGE_SHIFT)
#elif defined(LS1BSOC)
	#define MAIN_ADDRH(x)		((x) >> (32 - (PAGE_SHIFT - 1)))
	#define MAIN_ADDRL(x)		((x) << (PAGE_SHIFT - 1))	/* 不访问spare区时A11无效 */
	#define MAIN_SPARE_ADDRH(x)	((x) >> (32 - PAGE_SHIFT))
	#define MAIN_SPARE_ADDRL(x)	((x) << PAGE_SHIFT)
#endif

#define	GPIO_CONF1	0xbfd010c4
#define	GPIO_CONF2	0xbfd010c8
#define	GPIO_MUX	0xbfd00420
struct ls1x_nand_info {
	struct nand_chip	nand_chip;

	/* MTD data control*/
	unsigned int	buf_start;
	unsigned int	buf_count;
	/* NAND registers*/
	unsigned int	mmio_base;

	unsigned int	dma_desc;
	unsigned int	dma_desc_phys;
	size_t			dma_desc_size;

	unsigned char	*data_buff;
	unsigned int	data_buff_phys;
	size_t			data_buff_size;

	/* relate to the command */
//	int	use_ecc;	/* use HW ECC ? */
	unsigned int	cmd;

	unsigned int	seqin_column;
	unsigned int	seqin_page_addr;
};
#endif
