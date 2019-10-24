/*
#include <build_config.h>
#include <types.h>
#include <errno.h>
#include <stdlib.h>
#include "include/termio.h"

#include <ctype.h>
#include <pio.h>
#include "include/termio.h"
*/
#include <stdio.h>
#include "spi_flash.h"
#define UNCACHED_MEMORY_ADDR    0xa0000000
#define PHYS_TO_UNCACHED(x)     ((unsigned)(x) | UNCACHED_MEMORY_ADDR)

#define readb(addr) (*(volatile unsigned char*)(addr))
#define writeb(val, addr) (*(volatile unsigned char*)(addr) = (val))


static unsigned char flash_id[3];
/*
 *  Flash devices known by this code.
 */
struct fl_device fl_known_dev[] = {
	{ "sst25vf080",	0xbf, 0x8E, FL_PROTO_AMD, FL_CAP_DE,		//lxy
	1024*__KB, 64*__KB,  NULL, NULL},	//&fl_func_sst8 },
	{ "winb25x128",	0xef, 0x18, FL_PROTO_AMD, FL_CAP_DE,
	16384*__KB, 64*__KB,  NULL, NULL},	//&fl_func_winb },
	{ "winb25x64",	0xef, 0x17, FL_PROTO_AMD, FL_CAP_DE,
	8192*__KB, 64*__KB,  NULL, NULL},	//&fl_func_winb },
	{ "winb25x80",	0xef, 0x14, FL_PROTO_AMD, FL_CAP_DE,
	1024*__KB, 64*__KB,  NULL, NULL},	//&fl_func_winb },
	{ "winb25x40",	0xef, 0x13, FL_PROTO_AMD, FL_CAP_DE,
	512*__KB, 64*__KB,  NULL, NULL},	//&fl_func_winb },
	{ "winb25x20",	0xef, 0x12, FL_PROTO_AMD, FL_CAP_DE,
	256*__KB, 64*__KB,	NULL, NULL},	//&fl_func_winb },
	{ "winb25x10",	0xef, 0x11, FL_PROTO_AMD, FL_CAP_DE,
	128*__KB, 64*__KB,  NULL, NULL},	//&fl_func_winb },
    { 0 },
};

struct ls1x_spi spi0;
struct spi_device spi_flash;

static inline struct ls1x_spi *ls1x_spi_to_hw(struct spi_device *sdev)
{
	return sdev->hw;
}

static unsigned int ls1x_spi_div(struct spi_device *spi, unsigned int hz)
{
	struct ls1x_spi *hw = ls1x_spi_to_hw(spi);
	unsigned int div, div_tmp, bit;
	unsigned long clk;

	clk = hw->clk;
	div = DIV_ROUND_UP(clk, hz);

	if (div < 2)
		div = 2;

	if (div > 4096)
		div = 4096;

	bit = fls(div) - 1;
	switch(1 << bit) {
		case 16: 
			div_tmp = 2;
			if (div > (1<<bit)) {
				div_tmp++;
			}
			break;
		case 32:
			div_tmp = 3;
			if (div > (1<<bit)) {
				div_tmp += 2;
			}
			break;
		case 8:
			div_tmp = 4;
			if (div > (1<<bit)) {
				div_tmp -= 2;
			}
			break;
		default:
			div_tmp = bit - 1;
			if (div > (1<<bit)) {
				div_tmp++;
			}
			break;
	}

	return div_tmp;
}

static int ls1x_spi_setup(struct spi_device *spi)
{
	struct ls1x_spi *hw = ls1x_spi_to_hw(spi);
	unsigned char ret;

	/* 注意spi bit per word 控制器支持8bit */
//	bpw = t ? t->bits_per_word : spi->bits_per_word;

	if (spi->max_speed_hz != hw->speed_hz) {
		hw->speed_hz = spi->max_speed_hz;
		hw->div = ls1x_spi_div(spi, hw->speed_hz);
	}
	hw->mode = spi->mode & (SPI_CPOL | SPI_CPHA);

	ret = readb(hw->base + REG_SPCR);
	ret = ret & 0xf0;
	ret = ret | (hw->mode << 2) | (hw->div & 0x03);
	writeb(ret, hw->base + REG_SPCR);

	ret = readb(hw->base + REG_SPER);
	ret = ret & 0xfc;
	ret = ret | (hw->div >> 2);
	writeb(ret, hw->base + REG_SPER);

	return 0;
}

static void ls1x_spi_hw_init(struct ls1x_spi *hw)
{
	unsigned char val;

	/* 使能SPI控制器，master模式，使能或关闭中断 */
	writeb(0x53, hw->base + REG_SPCR);
	/* 清空状态寄存器 */
	writeb(0xc0, hw->base + REG_SPSR);
	/* 1字节产生中断，采样(读)与发送(写)时机同时 */
	writeb(0x03, hw->base + REG_SPER);
	/* 片选设置 */
	writeb(0xff, hw->base + REG_SOFTCS);
	/* 关闭SPI flash */
	val = readb(hw->base + REG_PARAM);
	val &= 0xfe;
	writeb(val, hw->base + REG_PARAM);
	/* SPI flash时序控制寄存器 */
	writeb(0x05, hw->base + REG_TIMING);
}
void ls1x_spi_chipselect(struct spi_device *spi, int is_active)
{
	struct ls1x_spi *hw = ls1x_spi_to_hw(spi);
	unsigned char ret;

	if (spi->chip_select != hw->chip_select) 
	{
		ls1x_spi_setup(spi);
		hw->chip_select = spi->chip_select;
//		printf("clk = %ld div = %d speed_hz = %d mode = %d\n", 
//			hw->clk, hw->div, hw->speed_hz, hw->mode);
	}

	ret = readb(hw->base + REG_SOFTCS);
	ret = (ret & 0xf0) | (0x01 << spi->chip_select);
	
	if (is_active) {
		ret = ret & (~(0x10 << spi->chip_select));
		writeb(ret, hw->base + REG_SOFTCS);
	} else {
		ret = ret | (0x10 << spi->chip_select);
		writeb(ret, hw->base + REG_SOFTCS);
	}
}

static inline void ls1x_spi_wait_rxe(struct ls1x_spi *hw)
{
	unsigned char ret;

	ret = readb(hw->base + REG_SPSR);
	ret = ret | 0x80;
	writeb(ret, hw->base + REG_SPSR);	/* Int Clear */

	ret = readb(hw->base + REG_SPSR);
	if (ret & 0x40) {
		writeb(ret & 0xbf, hw->base + REG_SPSR);	/* Write-Collision Clear */
	}
}

static inline void ls1x_spi_wait_txe(struct ls1x_spi *hw)
{
	while (1) {
		if (readb(hw->base + REG_SPSR) & 0x80) {
			break;
		}
	}
}

void ls1x_spi_writeb(struct spi_device *spi, unsigned char value)
{
	struct ls1x_spi *hw = ls1x_spi_to_hw(spi);

	writeb(value, hw->base + REG_TXFIFO);
	ls1x_spi_wait_txe(hw);
	readb(hw->base + REG_RXFIFO);
	ls1x_spi_wait_rxe(hw);
}
unsigned char ls1x_spi_readb(struct spi_device *spi)
{
	struct ls1x_spi *hw = ls1x_spi_to_hw(spi);
	unsigned char value;

	writeb(0, hw->base + REG_TXFIFO);
	ls1x_spi_wait_txe(hw);
	value = readb(hw->base + REG_RXFIFO);
	ls1x_spi_wait_rxe(hw);
	return value;
}
int read_status(void)
{
	int val;

	ls1x_spi_chipselect(&spi_flash, 1);
	ls1x_spi_writeb(&spi_flash, 0x05);
	val = ls1x_spi_readb(&spi_flash);
	ls1x_spi_chipselect(&spi_flash, 0);

	return val;
}

static void read_jedecid(unsigned char *p)
{
	int i;

	ls1x_spi_chipselect(&spi_flash, 1);
	ls1x_spi_writeb(&spi_flash, 0x9f);

	for (i=0; i<3; i++) {
		p[i] = ls1x_spi_readb(&spi_flash);
	}
	ls1x_spi_chipselect(&spi_flash, 0);
}


struct fl_device *findNorFlashType(void)
{
	struct fl_device *dev;
	read_jedecid(flash_id);
	
	for(dev = &fl_known_dev[0]; dev->fl_name != 0; dev++) 
	{
		if(dev->fl_mfg == (char)flash_id[0] && dev->fl_id == (char)flash_id[2]) 
		{
			printf("Nor-flash's name:%s,size:%dkB\n",dev->fl_name,dev->fl_size/__KB);
			return(dev);	/* GOT IT! */
		}
	}
	printf("Nor-flash's name:unknown,size's:unknown\n");
	return NULL;
}

int ls1x_spi_probe(void)
{
	spi0.base = LS1X_SPI0_BASE;
	spi0.clk = clk_get_apb_rate();
	
	ls1x_spi_hw_init(&spi0);

	spi_flash.hw = &spi0;
	spi_flash.max_speed_hz = 60000000;	/* Hz */
	spi_flash.mode = SPI_MODE_0;
	spi_flash.chip_select = 0;
	ls1x_spi_setup(&spi_flash);
	findNorFlashType();
	return 0;
}

int write_enable(void)
{
	while (read_status() & 0x01);

	ls1x_spi_chipselect(&spi_flash, 1);
	ls1x_spi_writeb(&spi_flash, 0x06);
	ls1x_spi_chipselect(&spi_flash, 0);

	return 1;
}

int write_status(char val)
{
	write_enable();
	while (read_status() & 0x01);

	ls1x_spi_chipselect(&spi_flash, 1);
	ls1x_spi_writeb(&spi_flash, 0x01);
	ls1x_spi_writeb(&spi_flash, val);
	ls1x_spi_chipselect(&spi_flash, 0);
	return 1;
}

int spi_flash_read_area(int flashaddr, char *buffer, int size)
{
	int i;

	ls1x_spi_chipselect(&spi_flash, 1);
	ls1x_spi_writeb(&spi_flash, 0x0b);
	ls1x_spi_writeb(&spi_flash, flashaddr >> 16);
	ls1x_spi_writeb(&spi_flash, flashaddr >> 8);
	ls1x_spi_writeb(&spi_flash, flashaddr);
	ls1x_spi_writeb(&spi_flash, 0x00);

	for (i=0; i<size; i++) 
	{
		*(buffer++) = ls1x_spi_readb(&spi_flash);
	}
	ls1x_spi_chipselect(&spi_flash, 0);

	while (read_status() & 0x01);
	return 0;
}

int spi_flash_erase_sector(unsigned int saddr, unsigned int eaddr, unsigned sectorsize)//0x1000 4k sector擦除 0x1000=4k=4096byte/1024
{
	unsigned int addr;
	for (addr=saddr; addr<eaddr; addr+=sectorsize) 
	{
		write_enable();
		write_status(0x00);
		while (read_status() & 0x01);
		write_enable();

		ls1x_spi_chipselect(&spi_flash, 1);
		ls1x_spi_writeb(&spi_flash, 0x20);
		ls1x_spi_writeb(&spi_flash, addr >> 16);
		ls1x_spi_writeb(&spi_flash, addr >> 8);
		ls1x_spi_writeb(&spi_flash, addr);
		ls1x_spi_chipselect(&spi_flash, 0);
		while (read_status() & 0x01);
	}
	while (read_status() & 0x01);
	return 0;
}

int spi_flash_erase_area(unsigned int saddr, unsigned int eaddr, unsigned sectorsize) //64KB block擦除 0x10000=64kB=65535byte/1024
{
	unsigned int addr;

	for (addr=saddr; addr<eaddr; addr+=sectorsize) {
		write_enable();
		write_status(0x00);
		while (read_status() & 0x01);
		write_enable();

		ls1x_spi_chipselect(&spi_flash, 1);
		ls1x_spi_writeb(&spi_flash, 0xd8);
		ls1x_spi_writeb(&spi_flash, addr >> 16);
		ls1x_spi_writeb(&spi_flash, addr >> 8);
		ls1x_spi_writeb(&spi_flash, addr);
		ls1x_spi_chipselect(&spi_flash, 0);
		while (read_status() & 0x01);
	}
	while (read_status() & 0x01);
	return 0;
}

static void spi_flash_write_byte(unsigned int addr, unsigned char data)
{
	unsigned char addr2, addr1, addr0;
	addr2 = (addr & 0xff0000)>>16;
	addr1 = (addr & 0x00ff00)>>8;
	addr0 = (addr & 0x0000ff);
	
	write_enable();
	while (read_status() & 0x01);

	ls1x_spi_chipselect(&spi_flash, 1);

	ls1x_spi_writeb(&spi_flash, 0x02);
	ls1x_spi_writeb(&spi_flash, addr2);
	ls1x_spi_writeb(&spi_flash, addr1);
	ls1x_spi_writeb(&spi_flash, addr0);
	ls1x_spi_writeb(&spi_flash, data);

	ls1x_spi_chipselect(&spi_flash, 0);
}

void read_flash_data(unsigned int addr,unsigned int read_len,char *buffer)
{
	if(read_len<1||buffer==0)
		return;
	spi_flash_read_area(addr, buffer, read_len);
}

int spi_flash_write_area(int flashaddr, char *buffer, int size)
{
	int i;

	write_status(0x00);
	for (i=0; size>0; flashaddr++,size--,i++) {
		spi_flash_write_byte(flashaddr, buffer[i]);
	}
	while (read_status() & 0x01);
	return 0;
}

int chipErase(int argc, char **argv)
{
	int i = 1;

	write_enable();
	while (read_status() & 0x01);

	ls1x_spi_chipselect(&spi_flash, 1);
	ls1x_spi_writeb(&spi_flash, 0xc7);
	ls1x_spi_chipselect(&spi_flash, 0);

	while (i++) 
	{
		if ((read_status() & 0x1) == 0x1) 
		{
			if(i % 10000 == 0)
				printf(".");
		} else 
		{
			printf("\nchipErase done...\n");
			break;
		}
	}
	return 1;
}












//====================================================================
//#include "../vm.h"
struct fl_map {
	unsigned int fl_map_base;	/* Start of flash area in physical map */
	unsigned int fl_map_size;	/* Size of flash area in physical map */
	int	fl_map_width;	/* Number of bytes to program in one cycle */
	int	fl_map_chips;	/* Number of chips to operate in one cycle */
	int	fl_map_bus;	/* Bus width type, se below */
	int	fl_map_offset;	/* Flash Offset mapped in memory */
	int fl_type;
};
#define	FL_BUS_8	0x01	/* Byte wide bus */
#define	FL_BUS_16	0x02	/* Short wide bus */
#define	FL_BUS_32	0x03	/* Word wide bus */
#define	FL_BUS_64	0x04	/* Quad wide bus */
#define	FL_BUS_8_ON_64	0x05	/* Byte wide on quad wide bus */
#define	FL_BUS_16_ON_64	0x06	/* 16-bit wide flash on quad wide bus */


#ifdef W25Q80
#define	TARGET_FLASH_DEVICES_16 \
	{ PHYS_TO_UNCACHED(0x1fc00000), 0x00100000, 1, 1, FL_BUS_8  },	\
	{ 0x00000000, 0x00000000 }
#elif W25Q128
#define	TARGET_FLASH_DEVICES_16 \
	{ PHYS_TO_UNCACHED(0x1fc00000), 0x01000000, 1, 1, FL_BUS_8  },	\
	{ 0x00000000, 0x00000000 }
#elif W25X64
#define	TARGET_FLASH_DEVICES_16 \
	{ PHYS_TO_UNCACHED(0x1fc00000), 0x00800000, 1, 1, FL_BUS_8  },	\
	{ 0x00000000, 0x00000000 }
#else
#define	TARGET_FLASH_DEVICES_16 \
	{ PHYS_TO_UNCACHED(0x1fc00000), 0x00080000, 1, 1, FL_BUS_8  },	\
	{ 0x00000000, 0x00000000 }	/* W25X40B 512KB */
#endif

struct fl_map tgt_fl_map_boot16[]={
	TARGET_FLASH_DEVICES_16
};


struct fl_map * tgt_flashmap(void)
{
	return tgt_fl_map_boot16;
}

struct fl_map *fl_find_map(void *base)
{
	struct fl_map *map;
	for(map = tgt_flashmap(); map->fl_map_size != 0; map++) {
		if(map->fl_map_base > (unsigned int)base ||
		   (map->fl_map_base + map->fl_map_size - 1) < (unsigned int)base) {
			continue;	/* Not this one */
		}
		else {
			return(map);
		}
	}
	return((struct fl_map *)NULL);
}




int fl_erase_device(void *fl_base, int size, int verbose)
{
	struct fl_map *map;
	int off;

	map = fl_find_map(fl_base);
	off = (int)(fl_base - map->fl_map_base) + map->fl_map_offset;
	spi_flash_erase_area(off, off+size, 0x10000);

	return 0;
}
static unsigned char init_id = 0;
static struct fl_device *nor_dev;

struct fl_device *fl_devident(void *base, struct fl_map **m)
{
	struct fl_device *dev;

	if(m)
		*m = fl_find_map(base);

	if (init_id == 0) {
		nor_dev = NULL;
		read_jedecid(flash_id);
		init_id = 1;

		for(dev = &fl_known_dev[0]; dev->fl_name != 0; dev++) {
			if(dev->fl_mfg == (char)flash_id[0] && dev->fl_id == (char)flash_id[2]) {
				nor_dev = dev;
				return(dev);	/* GOT IT! */
			}
		}
	}
	return nor_dev;
}
static int spi_flash_write_area_sst_AAI(int flashaddr, char *buffer, int size)
{
	unsigned char addr2, addr1, addr0;
	int count = size;
	addr2 = (flashaddr & 0xff0000)>>16;
	addr1 = (flashaddr & 0x00ff00)>>8;
	addr0 = (flashaddr & 0x0000ff);

	write_status(0x00);

	write_enable();
	while (read_status() & 0x01);

	ls1x_spi_chipselect(&spi_flash, 1);
	/*AAI command */
	ls1x_spi_writeb(&spi_flash, 0xad);
	ls1x_spi_writeb(&spi_flash, addr2);
	ls1x_spi_writeb(&spi_flash, addr1);
	ls1x_spi_writeb(&spi_flash, addr0);

	ls1x_spi_writeb(&spi_flash, buffer[0]);
	ls1x_spi_writeb(&spi_flash, buffer[1]);
	ls1x_spi_chipselect(&spi_flash, 0);

	while (read_status() & 0x01);

	count -= 2;	
	buffer += 2;
	while (count > 0) {
		ls1x_spi_chipselect(&spi_flash, 1);
		/*AAI command */
		ls1x_spi_writeb(&spi_flash, 0xad);
		ls1x_spi_writeb(&spi_flash, *(buffer++));
		ls1x_spi_writeb(&spi_flash, *(buffer++));
		ls1x_spi_chipselect(&spi_flash, 0);
		/* read status */
		while (read_status() & 0x01);

		count -= 2;
	}

	ls1x_spi_chipselect(&spi_flash, 1);
	ls1x_spi_writeb(&spi_flash, 0x04);
	ls1x_spi_chipselect(&spi_flash, 0);

	while (read_status() & 0x01);
}
static int spi_flash_write_area_sst_fast(int flashaddr, char *buffer, int size)
{
	int count = size;
	int temp;
	temp = count % 2;
	
	if (count % 2) {
		spi_flash_write_area(flashaddr, buffer, 1);
		count--;
		buffer++;
		flashaddr++;		
	}
	if (count != 0) {
		spi_flash_write_area_sst_AAI(flashaddr, buffer, count);
	}
}
static void spi_flash_write_byte_fast(unsigned int addr, char *data, unsigned int size)
{
	unsigned int i;
	unsigned char addr2, addr1, addr0;

	addr2 = (addr & 0xff0000)>>16;
	addr1 = (addr & 0x00ff00)>>8;
	addr0 = (addr & 0x0000ff);

	write_enable();
	while (read_status() & 0x01);

	ls1x_spi_chipselect(&spi_flash, 1);

	ls1x_spi_writeb(&spi_flash, 0x02);
	ls1x_spi_writeb(&spi_flash, addr2);
	ls1x_spi_writeb(&spi_flash, addr1);
	ls1x_spi_writeb(&spi_flash, addr0);

	/*send data(one byte)*/
	for (i=0; i<size; i++) {
		ls1x_spi_writeb(&spi_flash, data[i]);
	}
	ls1x_spi_chipselect(&spi_flash, 0);

	while (read_status() & 0x01);
}


static int spi_flash_write_area_fast(int flashaddr, char *buffer, int size)
{
	int i;

	write_status(0x00);

	for (i=0; size>0;) {
		if (size >= 256) {
			spi_flash_write_byte_fast(flashaddr, &buffer[i], 256);
			size -= 256;
			i += 256;
			flashaddr += 256;
		} else {
			spi_flash_write_byte_fast(flashaddr, &buffer[i], size);
			break;
		}
	}

	while (read_status() & 0x01);
	return 0;
}

int fl_program_device(void *fl_base, void *data_base, int data_size, int verbose)
{
	struct fl_map *map;
	int off;

#if 1
	fl_devident(fl_base, NULL);
	map = fl_find_map(fl_base);
	off = (int)(fl_base - map->fl_map_base) + map->fl_map_offset;
	if (nor_dev != NULL){
		if (nor_dev->fl_mfg == (char)0xbf) {
			printf (" byte write %s\n", nor_dev->fl_name);
			spi_flash_write_area_sst_fast(off, data_base, data_size);	/* SST */
		}
		else if (nor_dev->fl_mfg == (char)0xef) {
			printf (" byte write %s\n", nor_dev->fl_name);
			spi_flash_write_area_fast(off, data_base, data_size);		/* winbond */
		}
	}
	else {
		printf (" byte write unknow flash type\n");
		spi_flash_write_area(off, data_base, data_size);
	}
#else
	map = fl_find_map(fl_base);
	off = (int)(fl_base - map->fl_map_base) + map->fl_map_offset;
	spi_flash_write_area(off,data_base,data_size);
#endif

	return 0;
}
/* spi flash 读使能 */
void ls1x_spi_flash_ren(struct spi_device *spi, int en)
{
	struct ls1x_spi *hw = ls1x_spi_to_hw(spi);
	unsigned char val, spcr;

	val = readb(hw->base + REG_PARAM);
	if (en) {
		val |= 0x01;
//		spcr = readb(hw->base + REG_SPCR);
//		spcr &= ~0x40;
//		writeb(spcr, hw->base + REG_SPCR);
	} else {
		val &= 0xfe;
//		spcr = readb(hw->base + REG_SPCR);
//		spcr |= 0x40;
//		writeb(spcr, hw->base + REG_SPCR);
	}
	writeb(val, hw->base + REG_PARAM);
}

int tgt_flashwrite_enable(void)
{
#ifdef FLASH_READ_ONLY
	return 0;
#else
	return(1);
#endif
}
void tgt_flashwrite_disable(void)
{
}

/*
 *  Verify flash contents to ram contents.
 */
int fl_verify_device(void *fl_base, void *data_base, int data_size, int verbose)
{
	struct fl_map *map;
	struct fl_device *dev;
	void *fl_last;
	int ok;
	int i;


	dev = fl_devident(fl_base, &map);
	if(dev == NULL) {
		return(-3);	/* No flash device found at address */
	}

	if(data_size == -1 || (int)data_base == -1) {
		return(-4);		/* Bad parameters */
	}
	if((data_size + ((int)fl_base - map->fl_map_base)) > map->fl_map_size) {
		return(-4);	/* Size larger than device array */
	}

	if(verbose) {
		printf("Verifying FLASH. ");
	}

	for(i = 0; i < data_size; i += map->fl_map_width) {
		fl_last = fl_base;
		switch(map->fl_map_bus) {
		case FL_BUS_8:
			ok = (*((unsigned char *)fl_base) == *((unsigned char *)data_base)); fl_base += 1; data_base += 1;
			break;

		case FL_BUS_16:
			ok = (*(unsigned short *)fl_base == *((unsigned short *)data_base)); fl_base += 2; data_base += 2;
			break;

		case FL_BUS_32:
			ok = (*(unsigned int *)fl_base == *(unsigned int  *)data_base); fl_base += 4; data_base += 4;
			break;
		/*

		case FL_BUS_64:
			movequad(&widedata, fl_base);
			ok = (bcmp(data_base, (void *)&widedata, 8) == 0);
			data_base += 8;
			fl_base += 8;
			break; 
		
		case FL_BUS_8_ON_64:
			ok = (*((u_char *)map->fl_map_base +
				   (((int)fl_base - map->fl_map_base) << 3)) ==
				*(u_char *)data_base++);
			fl_base++;
			break;
			*/
		}

		if(verbose & !ok) {
			printf(" error offset %p\n", fl_last);
			{
				char str[100];
				printf("erase all chip(y/N)?");
				gets(str);
				if(str[0]=='y'||str[0]=='Y')
				{
					tgt_flashwrite_enable();
					printf("Erasing all FLASH blocks. ");
					fl_erase_device(map->fl_map_base,map->fl_map_size,0);
					tgt_flashwrite_disable();
				}
			}
			break;
		}
		else if(verbose) {
			dotik(32, 0);
		}
	}

	if(verbose && ok) {
		printf("\b No Errors found.\n");
	}
	
	return(ok);
}

void tgt_flashprogram(void *p, int size, void *s, int endian)
{
	extern struct spi_device spi_flash;

	printf("Programming flash %x:%x into %x\n", s, size, p);
	if(fl_erase_device(p, size, 1)) 
	{
		printf("Erase failed!\n");
		return;
	}
	if(fl_program_device(p, s, size, 1)) 
	{
		printf("Programming failed!\n");
	}

	ls1x_spi_flash_ren(&spi_flash, 1);
	fl_verify_device(p, s, size, 1);
	ls1x_spi_flash_ren(&spi_flash, 0);
}

void tgt_flashinfo(void *p, unsigned int *t)
{
	struct fl_map *map;

	map = fl_find_map(p);
	if(map) {
		*t = map->fl_map_size;
	}
	else {
		*t = 0;
	}
}


