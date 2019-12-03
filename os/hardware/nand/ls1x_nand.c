
#include <buildconfig.h>
#include <stdio.h>
#include <types.h>
#include <string.h>
#include <rtdef.h>
#include <filesys.h>

#include "mtd/mtd.h"
#include "mtd/nand.h"
#include "mtd/partitions.h"
#include "mtd/pio.h"

#include "ls1x_nand.h"

typedef unsigned int dma_addr_t;


struct mtd_info *ls1x_mtd = NULL;
static unsigned int order_addr_in;



static void ls1x_nand_ecc_hwctl(struct mtd_info *mtd, int mode)
{
	return;
}

static int ls1x_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *chip)
{
	return 0;
}

static void ls1x_nand_select_chip(struct mtd_info *mtd, int chip)
{
	return;
}

static int ls1x_nand_dev_ready(struct mtd_info *mtd)
{
	/* 澶氱墖flash鐨剅dy淇″彿濡備綍鍒ゆ柇锛?*/
	struct ls1x_nand_info *info = mtd->priv;
	unsigned int ret;
	ret = nand_readl(info, NAND_CMD) & 0x000f0000;
	if (ret) {
		return 1;
	}
	return 0;
}

static void inline ls1x_nand_start(struct ls1x_nand_info *info)
{
	nand_writel(info, NAND_CMD, nand_readl(info, NAND_CMD) | 0x1);
}

static void inline ls1x_nand_stop(struct ls1x_nand_info *info)
{
}
static int inline ls1x_nand_done(struct ls1x_nand_info *info)
{
	int ret, timeout = 400000;

	do 
	{
		ret = nand_readl(info, NAND_CMD);
		timeout--;
//		printk("NAND_CMD=0x%2X\n", nand_readl(info, NAND_CMD));
	} while (((ret & 0x400) != 0x400) && timeout);

	return timeout;
}
static void start_dma_nand(unsigned int flags, struct ls1x_nand_info *info)
{
//	int timeout = 5000;
//	unsigned long irq_flags;
	writel(0, info->dma_desc + DMA_ORDERED);
	writel(info->data_buff_phys, info->dma_desc + DMA_SADDR);//内存地址寄存器 往内存写入数据用
	writel(DMA_ACCESS_ADDR, info->dma_desc + DMA_DADDR);//设备地址寄存器 从设备读取数据
	writel((info->buf_count + 3) / 4, info->dma_desc + DMA_LENGTH);
	writel(0, info->dma_desc + DMA_STEP_LENGTH);
	writel(1, info->dma_desc + DMA_STEP_TIMES);

	if (flags)
	{
		writel(0x00001001, info->dma_desc + DMA_CMD);
	} 
	else 
	{
		writel(0x00000001, info->dma_desc + DMA_CMD);
	}

//	local_irq_save(irq_flags);

	ls1x_nand_start(info);	/* 使能nand命令 */

	writel((info->dma_desc_phys & ~0x1F) | 0x8, ORDER_ADDR_IN);	/* 启动DMA */
	while (readl(ORDER_ADDR_IN) & 0x8) 
	{ 
//		order_addr_in
//		rt_kprintf("xxx:%s. %x\n",__func__, readl(ORDER_ADDR_IN));
//		udelay(5);
//		timeout--;
	}
//	local_irq_restore(irq_flags);
	if (!ls1x_nand_done(info)) 
	{
		rt_kprintf("Wait time out!!!\n");
		ls1x_nand_stop(info);
	}
}
static void ls1x_nand_cmdfunc(struct mtd_info *mtd, unsigned command, int column, int page_addr)
{
	struct ls1x_nand_info *info = mtd->priv;

	switch (command) {
	case NAND_CMD_READOOB:
		info->buf_count = mtd->oobsize;
		info->buf_start = column;
		nand_writel(info, NAND_CMD, SPARE | READ);
		nand_writel(info, NAND_ADDR_L, MAIN_SPARE_ADDRL(page_addr) + mtd->writesize);
		nand_writel(info, NAND_ADDR_H, MAIN_SPARE_ADDRH(page_addr));
		nand_writel(info, NAND_OPNUM, info->buf_count);
		nand_writel(info, NAND_PARAM, (nand_readl(info, NAND_PARAM) & 0xc000ffff) | (info->buf_count << 16));
		start_dma_nand(0, info);
		break;
	case NAND_CMD_READ0:
		info->buf_count = mtd->writesize + mtd->oobsize;
		info->buf_start = column;
		DEBUG_YA5(page_addr);
		//DEBUG_YA5(info->buf_count);
		nand_writel(info, NAND_CMD, SPARE | MAIN | READ);
		nand_writel(info, NAND_ADDR_L, MAIN_SPARE_ADDRL(page_addr));
		nand_writel(info, NAND_ADDR_H, MAIN_SPARE_ADDRH(page_addr));
		nand_writel(info, NAND_OPNUM, info->buf_count);
		nand_writel(info, NAND_PARAM, (nand_readl(info, NAND_PARAM) & 0xc000ffff) | (info->buf_count << 16)); /* 1C娉ㄦ剰 */
		start_dma_nand(0, info);
		break;
	case NAND_CMD_SEQIN:
		info->buf_count = mtd->writesize + mtd->oobsize - column;
		info->buf_start = 0;
		info->seqin_column = column;
		info->seqin_page_addr = page_addr;
		break;
	case NAND_CMD_PAGEPROG:
		if (info->seqin_column < mtd->writesize)
			nand_writel(info, NAND_CMD, SPARE | MAIN | WRITE);
		else
			nand_writel(info, NAND_CMD, SPARE | WRITE);
		nand_writel(info, NAND_ADDR_L, MAIN_SPARE_ADDRL(info->seqin_page_addr) + info->seqin_column);
		nand_writel(info, NAND_ADDR_H, MAIN_SPARE_ADDRH(info->seqin_page_addr));
		nand_writel(info, NAND_OPNUM, info->buf_count);
		nand_writel(info, NAND_PARAM, (nand_readl(info, NAND_PARAM) & 0xc000ffff) | (info->buf_count << 16)); /* 1C娉ㄦ剰 */
		start_dma_nand(1, info);
		break;
	case NAND_CMD_RESET:
		info->buf_count = 0x0;
		info->buf_start = 0x0;
		nand_writel(info, NAND_CMD, RESET);
		ls1x_nand_start(info);
		if (!ls1x_nand_done(info)) {
			rt_kprintf("Wait time out!!!\n");
			ls1x_nand_stop(info);
		}
		break;
	case NAND_CMD_ERASE1:
		info->buf_count = 0x0;
		info->buf_start = 0x0;
		nand_writel(info, NAND_ADDR_L, MAIN_ADDRL(page_addr));
		nand_writel(info, NAND_ADDR_H, MAIN_ADDRH(page_addr));
		nand_writel(info, NAND_OPNUM, 0x01);
		nand_writel(info, NAND_PARAM, (nand_readl(info, NAND_PARAM) & 0xc000ffff) | (0x1 << 16)); /* 1C娉ㄦ剰 */
		nand_writel(info, NAND_CMD, ERASE);
		ls1x_nand_start(info);
		if (!ls1x_nand_done(info)) {
			rt_kprintf("Wait time out!!!\n");
			ls1x_nand_stop(info);
		}
		break;
	case NAND_CMD_STATUS:
		info->buf_count = 0x1;
		info->buf_start = 0x0;
		nand_writel(info, NAND_CMD, READ_STATUS);
		ls1x_nand_start(info);
		if (!ls1x_nand_done(info)) {
			rt_kprintf("Wait time out!!!\n");
			ls1x_nand_stop(info);
		}
		*(info->data_buff) = (nand_readl(info, NAND_IDH) >> 8) & 0xff;
		*(info->data_buff) |= 0x80;
		break;
	case NAND_CMD_READID:
		info->buf_count = 0x5;
		info->buf_start = 0;
		nand_writel(info, NAND_CMD, READ_ID);
		ls1x_nand_start(info);
		if (!ls1x_nand_done(info)) {
			rt_kprintf("Wait time out!!!\n");
			ls1x_nand_stop(info);
			break;
		}
		*(info->data_buff) = nand_readl(info, NAND_IDH);
		*(info->data_buff + 1) = (nand_readl(info, NAND_IDL) >> 24) & 0xff;
		*(info->data_buff + 2) = (nand_readl(info, NAND_IDL) >> 16) & 0xff;
		*(info->data_buff + 3) = (nand_readl(info, NAND_IDL) >> 8) & 0xff;
		*(info->data_buff + 4) = nand_readl(info, NAND_IDL) & 0xff;
		break;
	case NAND_CMD_ERASE2:
//	case NAND_CMD_READ1:
		info->buf_count = 0x0;
		info->buf_start = 0x0;
		break;
	default :
		info->buf_count = 0x0;
		info->buf_start = 0x0;
		rt_kprintf("non-supported command.\n");
		break;
	}

	nand_writel(info, NAND_CMD, 0x00);
}
static u16 ls1x_nand_read_word(struct mtd_info *mtd)
{
	struct ls1x_nand_info *info = mtd->priv;
	u16 retval = 0xFFFF;

	if (!(info->buf_start & 0x1) && info->buf_start < info->buf_count) {
		retval = *(u16 *)(info->data_buff + info->buf_start);
		info->buf_start += 2;
	}
	return retval;
}
static uint8_t ls1x_nand_read_byte(struct mtd_info *mtd)
{
	struct ls1x_nand_info *info = mtd->priv;
	char retval = 0xFF;

	if (info->buf_start < info->buf_count)
		retval = info->data_buff[(info->buf_start)++];

	return retval;
}
static void ls1x_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct ls1x_nand_info *info = mtd->priv;
	int real_len = min_t(size_t, len, info->buf_count - info->buf_start);

	memcpy(buf, info->data_buff + info->buf_start, real_len);
	info->buf_start += real_len;
}

static void ls1x_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct ls1x_nand_info *info = mtd->priv;
	int real_len = min_t(size_t, len, info->buf_count - info->buf_start);

	memcpy(info->data_buff + info->buf_start, buf, real_len);
	info->buf_start += real_len;
}
static int ls1x_nand_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	int i = 0;

	while (len--) {
		if (buf[i++] != ls1x_nand_read_byte(mtd) ) {
			rt_kprintf("verify error..., i= %d !\n\n", i-1);
			return -1;
		}
	}
	return 0;
}
static void * dma_alloc_coherent(size_t size,dma_addr_t * dma_desc_phys)
{
	void * vstr=NULL;
	unsigned int vtr;
	unsigned int phy;
	vstr=(void *)malloc_align(size,4096);
	
	memset(vstr,0,size);
	if(vstr==NULL)
	{
		rt_kprintf("dma_alloc_coherent() error\n");
		return NULL;
	}
	vtr=(unsigned int)vstr;
	phy=vtr&0xfffffff;//|0xa0000000;
	*dma_desc_phys=phy;
	vtr=(unsigned int)vstr;
	phy=vtr&0xfffffff|0xa0000000;
	//rt_kprintf("dma_alloc_coherent() phy:0x%08X\n",phy);
	return (void *)phy;
}

static int ls1x_nand_init_buff(struct ls1x_nand_info *info)
{
	/* DMA鎻忚堪绗﹀湴鍧? */
	info->dma_desc_size = 4096;//ALIGN(DMA_DESC_NUM, PAGE_SIZE);	/* 鐢宠鍐呭瓨澶у皬锛岄〉瀵归綈 */
	info->dma_desc=(unsigned int)dma_alloc_coherent(info->dma_desc_size,&info->dma_desc_phys);
	if(info->dma_desc == NULL)
		return -1;
	//info->dma_desc_phys = (unsigned int)(info->dma_desc) & 0x1fffffff;

	/* NAND鐨凞MA鏁版嵁缂撳瓨 */
	info->data_buff_size = 8192;//ALIGN(MAX_BUFF_SIZE, PAGE_SIZE);	/* 鐢宠鍐呭瓨澶у皬锛岄〉瀵归綈 */
	info->data_buff=dma_alloc_coherent(info->data_buff_size,&info->data_buff_phys);
	if(info->data_buff == NULL)
		return -1;
	//info->data_buff_phys = (unsigned int)(info->data_buff) & 0x1fffffff;

	order_addr_in = ORDER_ADDR_IN;

	rt_kprintf("data_buff_addr:0x%08x, dma_desc_addr:0x%08x\n", info->data_buff, info->dma_desc);
	
	return 0;
}

static void nand_gpio_init(void)
{
#ifdef LS1ASOC
{
	int val;
#ifdef NAND_USE_LPC //NAND澶嶇敤LPC PWM01
	val = __raw_readl(GPIO_MUX);
	val |= 0x2a000000;
	__raw_writel(val, GPIO_MUX);

	val = __raw_readl(GPIO_CONF2);
	val &= ~(0xffff<<6);			//nand_D0~D7 & nand_control pin
	__raw_writel(val, GPIO_CONF2);
#elif NAND_USE_SPI1 //NAND澶嶇敤SPI1 PWM23
	val = __raw_readl(GPIO_MUX);
	val |= 0x14000000;
	__raw_writel(val, GPIO_MUX);

	val = __raw_readl(GPIO_CONF1);
	val &= ~(0xf<<12);				//nand_D0~D3
	__raw_writel(val, GPIO_CONF1);

	val = __raw_readl(GPIO_CONF2);
	val &= ~(0xfff<<12);			//nand_D4~D7 & nand_control pin
	__raw_writel(val, GPIO_CONF2);
#endif
}
#endif
}
static void ls1x_nand_init_hw(struct ls1x_nand_info *info)
{
	nand_writel(info, NAND_CMD, 0x00);
	nand_writel(info, NAND_ADDR_L, 0x00);
	nand_writel(info, NAND_ADDR_H, 0x00);
	nand_writel(info, NAND_TIMING, (HOLD_CYCLE << 8) | WAIT_CYCLE);
#if defined(LS1ASOC) || defined(LS1BSOC)
	nand_writel(info, NAND_PARAM, 0x00000100);	/* 璁剧疆澶栭儴棰楃矑澶у皬锛?A 2Gb? */
#elif defined(LS1CSOC)
	nand_writel(info, NAND_PARAM, 0x08005000);
#endif
	nand_writel(info, NAND_OPNUM, 0x00);
	nand_writel(info, NAND_CS_RDY, 0x88442211);	/* 閲嶆槧灏剅dy1/2/3淇″彿鍒皉dy0 rdy鐢ㄤ簬鍒ゆ柇鏄惁蹇?*/
}


static int ls1x_nand_scan(struct mtd_info *mtd)
{
	struct ls1x_nand_info *info = mtd->priv;
	struct nand_chip *chip = (struct nand_chip *)info;
	uint64_t chipsize;
	int exit_nand_size;

	int ret=nand_scan_ident(mtd,1);
	//这里有时候会莫名其妙出现！=2048的情况所以多试几次
	if (mtd->writesize!=2048||ret)
	{
		rt_kprintf("There should be no mistakes.ls1x_nand_scan try 3 times,maybe success...\n");
		int i=0;
		for(i=0;i<3;i++)
		{
			delay_ms(1000);
			ret=nand_scan_ident(mtd,1);
			if(!ret&&mtd->writesize==2048)
			{
				rt_kprintf("ok, try successful. mtd->writesize=%d\n",mtd->writesize);
				goto checkok;
			}
			rt_kprintf("this is %d time.mtd->writesize=%d\n",i,mtd->writesize);
			
		}
		return -ENODEV;
	}
	
checkok:
	chipsize = (chip->chipsize << 3) >> 20;	/* Mbit */

	switch (mtd->writesize) 
	{
	case 2048:
		switch (chipsize) 
		{
		case 1024:
		#if defined(LS1ASOC)
			exit_nand_size = 0x1;
		#else
			exit_nand_size = 0x0;
		#endif
			break;
		case 2048:
			exit_nand_size = 0x1; break;
		case 4096:
			exit_nand_size = 0x2; break;
		case 8192:
			exit_nand_size = 0x3; break;
		default:
			exit_nand_size = 0x3; break;
		}
		break;
	case 4096:
		exit_nand_size = 0x4; break;
	case 8192:
		switch (chipsize) {
		case 32768:
			exit_nand_size = 0x5; break;
		case 65536:
			exit_nand_size = 0x6; break;
		case 131072:
			exit_nand_size = 0x7; break;
		default:
			exit_nand_size = 0x8; break;
		}
		break;
	case 512:
		switch (chipsize) 
		{
		case 64:
			exit_nand_size = 0x9; break;
		case 128:
			exit_nand_size = 0xa; break;
		case 256:
			exit_nand_size = 0xb; break;
		case 512:
			exit_nand_size = 0xc;break;
		default:
			exit_nand_size = 0xd; break;
		}
		break;
	default:
		rt_kprintf(" exit nand size error! mtd->writesize:%d\n",mtd->writesize);
		return -ENODEV;
	}
	nand_writel(info, NAND_PARAM, (nand_readl(info, NAND_PARAM) & 0xfffff0ff) | (exit_nand_size << 8));
	chip->cmdfunc(mtd, NAND_CMD_RESET, 0, 0);
	return nand_scan_tail(mtd);
}

int ls1x_nand_init(void *parameter)
{
	struct ls1x_nand_info *info;
	struct nand_chip *chip;

	rt_kprintf("\nLs1x hardware Nand detecting...\n");

	/* Allocate memory for MTD device structure and private data */
	ls1x_mtd = malloc(sizeof(struct mtd_info) + sizeof(struct ls1x_nand_info));
	if (!ls1x_mtd) 
	{
		rt_kprintf("Unable to allocate NAND MTD device structure.\n");
		return -ENOMEM;
	}
	DEBUG_YA5(0);
	info = (struct ls1x_nand_info *)(&ls1x_mtd[1]);
	chip = (struct nand_chip *)(&ls1x_mtd[1]);
	memset(ls1x_mtd, 0, sizeof(struct mtd_info));
	memset(info, 0, sizeof(struct ls1x_nand_info));
	DEBUG_YA5(0);
	ls1x_mtd->priv = info;
//	chip->chip_delay = 15;	/* 15 us command delay time 浠庢暟鎹墜鍐岃幏鐭ュ懡浠ゅ欢杩熸椂闂?*/

	chip->options		= NAND_CACHEPRG;
//	chip->ecc.mode		= NAND_ECC_NONE;
	chip->ecc.mode		= NAND_ECC_SOFT;
//	chip->controller	= &info->controller;
	chip->waitfunc		= ls1x_nand_waitfunc;
	chip->select_chip	= ls1x_nand_select_chip;
	chip->dev_ready		= ls1x_nand_dev_ready;
	chip->cmdfunc		= ls1x_nand_cmdfunc;
	chip->read_word		= ls1x_nand_read_word;
	chip->read_byte		= ls1x_nand_read_byte;
	chip->read_buf		= ls1x_nand_read_buf;
	chip->write_buf		= ls1x_nand_write_buf;
	chip->verify_buf	= ls1x_nand_verify_buf;

	info->mmio_base = LS1X_NAND_BASE;

	if (ls1x_nand_init_buff(info)) 
	{
		rt_kprintf("\n\nerror: PMON nandflash driver have some error!\n\n");
		return -ENXIO;
	}
	//DEBUG_YA5(0);
	nand_gpio_init();
	ls1x_nand_init_hw(info);

	/* Scan to find existence of the device */
	int ret=ls1x_nand_scan(ls1x_mtd);
	//DEBUG_YA6(ret);
	if (ret) 
	{
		DEBUG_YA6(ret);
		free(ls1x_mtd);
		return -ENXIO;
	}

	/*
	rt_kprintf("mtd_info:size:%d\n erasesize:%d\n oobblock:%d\n oobsize:%d\n writesize:%d\n",
		ls1x_mtd->size,ls1x_mtd->erasesize,ls1x_mtd->oobblock,ls1x_mtd->oobsize,ls1x_mtd->writesize);
	*/
	/* Register the partitions */
	ls1x_mtd->name = "ls1x-nand";

#ifdef MTDPARTS
	mtdpart_setup_real(getenv("mtdparts"));
#else
	add_mtd_device(ls1x_mtd, 0, 14*1024*1024, "kernel");
	add_mtd_device(ls1x_mtd, 14*1024*1024, 100*1024*1024, "rootfs");
	add_mtd_device(ls1x_mtd, (100+14)*1024*1024, 14*1024*1024, "data");
#endif


	THREAD_DEAD_EXIT;	
	return 0; 
}


