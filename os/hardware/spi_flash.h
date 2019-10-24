#define DIV_ROUND_UP(n, d)      (((n) + (d) - 1) / (d))

#define LS1X_SPI0_BASE			0xbfe80000
#define LS1X_SPI1_BASE			0xbfec0000

#define REG_SPCR		0x00	//控制寄存器
#define REG_SPSR		0x01	//状态寄存器
#define REG_TXFIFO		0x02	//数据传输寄存器 输出
#define REG_RXFIFO		0x02	//数据传输寄存器 输入
#define REG_SPER		0x03	//外部寄存器
#define REG_PARAM		0x04	//SPI Flash参数控制寄存器
#define REG_SOFTCS		0x05	//SPI Flash片选控制寄存器
#define REG_TIMING		0x06	//SPI Flash时序控制寄存器


struct ls1x_spi {
	unsigned int base;
	unsigned char chip_select;
	unsigned long clk;
	unsigned int div;
	unsigned int speed_hz;
	unsigned int mode;
};

struct spi_device {
	struct ls1x_spi *hw;

	unsigned int	max_speed_hz;
	unsigned char	chip_select;
	unsigned char			mode;
#define	SPI_CPHA	0x01			/* clock phase */
#define	SPI_CPOL	0x02			/* clock polarity */
#define	SPI_MODE_0	(0|0)			/* (original MicroWire) */
#define	SPI_MODE_1	(0|SPI_CPHA)
#define	SPI_MODE_2	(SPI_CPOL|0)
#define	SPI_MODE_3	(SPI_CPOL|SPI_CPHA)
#define	SPI_CS_HIGH	0x04			/* chipselect active high? */
#define	SPI_LSB_FIRST	0x08			/* per-word bits-on-wire */
#define	SPI_3WIRE	0x10			/* SI/SO signals shared */
#define	SPI_LOOP	0x20			/* loopback mode */
#define	SPI_NO_CS	0x40			/* 1 dev/bus, no chipselect */
#define	SPI_READY	0x80			/* slave pulls low to pause */
	unsigned char			bits_per_word;
	int			irq;
};

struct fl_functions {
	int	(*erase_chip)		/* Erase Chip */
		(struct fl_map *, struct fl_device *);
	int	(*erase_sector)		/* Erase Sector */
		(struct fl_map *, struct fl_device *, int);
	int	(*isbusy)		/* Check if device is Busy */
		(struct fl_map *, struct fl_device *, int, int, int);
	int	(*reset)		/* Reset Chip */
		(struct fl_map *, struct fl_device *);
	int	(*erase_suspend)		/* Erase Suspend */
		(struct fl_map *, struct fl_device *);
	int	(*program)		/* Program Device */
		(struct fl_map *, struct fl_device *, int, unsigned char *);
};

struct fl_device {
	char	*fl_name;
	char	fl_mfg;		/* Flash manufacturer code */
	char	fl_id;		/* Device id code */
	char	fl_proto;	/* Command protocol */
	char	fl_cap;		/* Flash capabilites */
	int	fl_size;	/* Device size in k-bytes */
	int	fl_secsize;	/* Device sector-size in k-bytes */
    int	*fl_varsecsize;	/* Not NULL, if device has flexable sector size */
	struct fl_functions *functions;
};

#define __KB 1024
#define	FL_PROTO_INT	1	/* Scalable Command Set */
#define	FL_PROTO_AMD	2	/* AMD in lack of better name... */
#define FL_PROTO_SST	2
#define FL_PROTO_ST		2
#define FL_PROTO_WINBOND	2
#define	FL_CAP_DE	0x01	/* Device have entire device Erase */
#define	FL_CAP_A7	0x02	/* Device uses a7 for Bulk Erase */

extern int bootRomSize;
extern struct ls1x_spi spi0;
extern struct spi_device spi_flash;

int chipErase(int argc, char **argv);
int bootSpaceErase(int argc, char **argv);
int readSpiFlashHex(int ac, char **argv);
int ls1x_spi_probe(void);
int envSpaceErase(int argc, char **argv);
struct fl_device *findNorFlashType(void);


