#include <buildconfig.h>
#include <types.h>
#include <stdio.h>
#include <filesys.h>


#define DC_BASE_ADDR0 0xbc301240
#define DC_BASE_ADDR1 0xbc301250
#define LS1X_CLK_PLL_FREQ	0xbfe78030
#define LS1X_CLK_PLL_DIV		0xbfe78034

#define writel(val, addr) (*(volatile unsigned long *)(addr) = (val))
#define readl(addr) (*(volatile unsigned long *)(addr))
#define PAGE_SIZE	(1 << 12)
#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)
#define ALIGN(x, a)		__ALIGN_KERNEL((x), (a))
#define __ALIGN_KERNEL(x, a)		__ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask)	(((x) + (mask)) & ~(mask))

#define FB_XSIZE 480
#define FB_YSIZE 272

static int fb_xsize, fb_ysize, frame_rate;
unsigned long fb_addr;

#if defined(LS1ASOC) || defined(LS1BSOC)
#define BURST_SIZE	0xff
static char *addr_cursor = NULL;
static char *mem_ptr = NULL;
#elif defined(LS1CSOC)
#define BURST_SIZE	0x7f
static char *addr_cursor = NULL;
static char *mem_ptr = NULL;

#define DIV_DC_EN			(0x1 << 31)
#define DIV_DC				(0x7f << 24)
#define DIV_CAM_EN			(0x1 << 23)
#define DIV_CAM			(0x7f << 16)
#define DIV_CPU_EN			(0x1 << 15)
#define DIV_CPU				(0x7f << 8)
#define DIV_DC_SEL_EN			(0x1 << 5)
#define DIV_DC_SEL				(0x1 << 4)
#define DIV_CAM_SEL_EN			(0x1 << 3)
#define DIV_CAM_SEL				(0x1 << 2)
#define DIV_CPU_SEL_EN			(0x1 << 1)
#define DIV_CPU_SEL				(0x1 << 0)

#define DIV_DC_SHIFT			24
#define DIV_CAM_SHIFT			16
#define DIV_CPU_SHIFT			8

#endif

static struct vga_struc {
	unsigned int pclk, refresh;
	unsigned int hr, hss, hse, hfl;
	unsigned int vr, vss, vse, vfl;
	unsigned int pan_config;		/* 注意 不同的lcd面板该值可能不同 */
	unsigned int hvsync_polarity;	/* 注意 不同的lcd面板该值可能不同 */
} vgamode[] = {
	{/*"240x320_70.00"*/	6429,	70,	240,	250,	260,	280,	320,	324,	326,	328,	0x00000301, 0x40000000},/* ili9341 DE mode */
	{/*"320x240_60.00"*/	7154,	60,	320,	332,	364,	432,	240,	248,	254,	276,	0x00000103, 0xc0000000},/* HX8238-D控制器 */
//	{/*"320x240_60.00"*/	6438,	60,	320,	336,	337,	408,	240,	250,	251,	263,	0x80001311, 0xc0000000},/* NT39016D控制器 */
	{/*"320x480_60.00"*/	12908,	60,	320,	360,	364,	432,	480,	488,	490,	498,	0x00000101, 0xc0000000},/* NT35310控制器 */
//	{/*"480x272_60.00"*/	9009,	60,	480,	0,	0,	525,	272,	0,	0,	288,	0x00000101, 0x00000000},/* AT043TN13 DE mode */
	{/*"480x272_60.00"*/	9009,	60,	480,	488,	489,	531,	272,	276,	282,	288,	0x00000101, 0xc0000000},/* LT043A-02AT */
	{/*"480x640_60.00"*/	20217,	60,	480,    488,    496,    520,    640,    642,    644,    648,	0x00000101, 0xc0000000},/* jbt6k74控制器 */
//	{/*"640x480_60.00"*/	25200,	60,	640,	656,	666,	800,	480,	512,    514,    525,	0x00000101, 0xc0000000},/* AT056TN52 */
	{/*"640x480_60.00"*/	25200,	60,	640,	656,	666,	800,	480,	512,    514,    525,	0x00000301, 0xc0000000},/* AT056TN53 */
	{/*"640x640_60.00"*/	33100,	60,	640,	672,	736,	832,	640,	641,	644,	663,	0x00000101, 0xc0000000},
	{/*"640x768_60.00"*/	39690,	60,	640,	672,	736,	832,	768,	769,	772,	795,	0x00000101, 0xc0000000},
	{/*"640x800_60.00"*/	42130,	60,	640,	680,	744,	848,	800,	801,	804,	828,	0x00000101, 0xc0000000},
	{/*"800x480_60.00"*/	29232,	60,	800,	0,	0,	928,	480,	0,    0,    525,	0x00000101, 0x00000000},/* AT070TN93 HX8264 DE mode */
//	{/*"800x480_60.00"*/	29232,	60,	800,	840,	888,	928,	480,	493,    496,    525,	0x00000100, 0xc0000000},/* HX8264 HV mode */

//	{/*"800x600_60.00"*/	39790,	60,	800,	840,	968,	1056,	600,	601,	605,	628,	0x00000101, 0xc0000000},/* VESA */
	{/*"800x600_75.00"*/	49500,	75,	800,	816,	896,	1056,	600,	601,	604,	625,	0x00000101, 0xc0000000},/* VESA */
	{/*"800x640_60.00"*/	40730,	60,	800,	832,	912,	1024,	640,	641,	644,	663,	0x00000101, 0xc0000000},
	{/*"832x600_60.00"*/	40010,	60,	832,	864,	952,	1072,	600,	601,	604,	622,	0x00000101, 0xc0000000},
	{/*"832x608_60.00"*/	40520,	60,	832,	864,	952,	1072,	608,	609,	612,	630,	0x00000101, 0xc0000000},
	{/*"1024x480_60.00"*/	38170,	60,	1024,	1048,	1152,	1280,	480,	481,	484,	497,	0x00000101, 0xc0000000},
	{/*"1024x600_60.00"*/	51206,	60,	1024,	0,	0,	1344,	600,	0,	0,	635,	0x00000101, 0x00000000}, /* HDBO101XLE-21 DE mode */
//	{/*"1024x600_60.00"*/	51206,	60,	1024,	1184,	1185,	1344,	600,	612,	613,	635,	0x00000101, 0xc0000000}, /* HDBO101XLE-21 HV mode */
	{/*"1024x640_60.00"*/	52830,	60,	1024,	1072,	1176,	1328,	640,	641,	644,	663,	0x00000101, 0xc0000000},
	{/*"1024x768_60.00"*/	65000,	60,	1024,	1048,	1184,	1344,	768,	771,	777,	806,	0x00000101, 0xc0000000},/* VESA */
	{/*"1152x764_60.00"*/	71380,	60,	1152,	1208,	1328,	1504,	764,	765,    768,    791,	0x00000101, 0xc0000000},
	{/*"1280x800_60.00"*/	83460,	60,	1280,	1344,	1480,	1680,	800,	801,    804,    828,	0x00000101, 0xc0000000},
//	{/*"1280x1024_60.00"*/	108000,	60,	1280,	1328,	1440,	1688,	1024,	1025,   1028,   1066,	0x00000101, 0xc0000000},/* VESA */
	{/*"1280x1024_75.00"*/	135000,	75,	1280,	1296,	1440,	1688,	1024,	1025,   1028,   1066,	0x00000101, 0xc0000000},/* VESA */
	{/*"1360x768_60.00"*/	85500,	60,	1360,	1424,	1536,	1792,	768,	771,    777,    795,	0x00000101, 0xc0000000},/* VESA */
	{/*"1440x1050_60.00"*/	121750,	60,	1440,	1528,	1672,	1904,	1050,	1053,	1057,	1089,	0x00000101, 0xc0000000},/* VESA */
//	{/*"1440x900_60.00"*/	106500,	60,	1440,	1520,	1672,	1904,	900,    903,    909,    934,	0x00000101, 0xc0000000},/* VESA */
	{/*"1440x900_75.00"*/	136750,	75,	1440,	1536,	1688,	1936,	900,	903,	909,	942,	0x00000101, 0xc0000000},/* VESA */
	{/*"1920x1080_60.00"*/	148500,	60,	1920,	2008,	2052,	2200,	1080,	1084,	1089,	1125,	0x00000101, 0xc0000000},/* VESA */
};

enum {
	OF_BUF_CONFIG = 0,
	OF_BUF_ADDR0 = 0x20,
	OF_BUF_STRIDE = 0x40,
	OF_BUF_ORIG = 0x60,
	OF_DITHER_CONFIG = 0x120,
	OF_DITHER_TABLE_LOW = 0x140,
	OF_DITHER_TABLE_HIGH = 0x160,
	OF_PAN_CONFIG = 0x180,
	OF_PAN_TIMING = 0x1a0,
	OF_HDISPLAY = 0x1c0,
	OF_HSYNC = 0x1e0,
	OF_VDISPLAY = 0x240,
	OF_VSYNC = 0x260,
	OF_BUF_ADDR1 = 0x340,
};


unsigned long tgt_pllfreq(void)
{
	return clk_get_pll_rate();
}

#include "video_fb.h"
static GraphicDevice *pGD,GD;	/* Pointer to Graphic array */
static void *video_fb_address;		/* frame buffer address */
static void *video_console_address;	/* console buffer start address */
static int console_col = 0; /* cursor col */
static int console_row = 0; /* cursor row */
unsigned int eorx, fgx, bgx;  /* color pats */

#define VIDEO_VISIBLE_COLS	(pGD->winSizeX)
#define VIDEO_VISIBLE_ROWS	(pGD->winSizeY)
#define VIDEO_PIXEL_SIZE	(pGD->gdfBytesPP)
#define VIDEO_DATA_FORMAT	(pGD->gdfIndex)
#define VIDEO_FB_ADRS		(pGD->frameAdrs)

void video_set_lut (
    unsigned int index,           /* color number */
    unsigned char r,              /* red */
    unsigned char g,              /* green */
    unsigned char b               /* blue */
    )
{
}

#define CONFIG_FB_DYN
#define CONFIG_VIDEO_LOGO
#define MEM_PRINTTO_VIDEO

#define X480x272
#define CONFIG_VIDEO_16BPP

static char *memfb;

#define VIDEO_COLS		VIDEO_VISIBLE_COLS
#define VIDEO_ROWS		VIDEO_VISIBLE_ROWS

#define LINUX_LOGO_WIDTH	80
#define LINUX_LOGO_HEIGHT	80
#define LINUX_LOGO_COLORS	214
#define LINUX_LOGO_LUT_OFFSET	0x20
#define __initdata
#include "linux_logo.h"
#define VIDEO_LOGO_WIDTH	LINUX_LOGO_WIDTH
#define VIDEO_LOGO_HEIGHT	LINUX_LOGO_HEIGHT
#define VIDEO_LOGO_LUT_OFFSET	LINUX_LOGO_LUT_OFFSET
#define VIDEO_LOGO_COLORS	LINUX_LOGO_COLORS

#define VIDEO_FONT_HEIGHT	16
#define VIDEO_FONT_WIDTH	8


#ifdef	CONFIG_VIDEO_LOGO
#define CONSOLE_ROWS		((VIDEO_ROWS - VIDEO_LOGO_HEIGHT) / VIDEO_FONT_HEIGHT)
#else
#define CONSOLE_ROWS		(VIDEO_ROWS / VIDEO_FONT_HEIGHT)
#endif

#define CONSOLE_COLS		(VIDEO_COLS / VIDEO_FONT_WIDTH)

#define VIDEO_SIZE		(VIDEO_ROWS*VIDEO_COLS*VIDEO_PIXEL_SIZE)
#define VIDEO_LINE_LEN		(VIDEO_COLS*VIDEO_PIXEL_SIZE)

#define BYTESWAP32(x)    ((((x) & 0x000000ff) <<  24) | (((x) & 0x0000ff00) << 8)|\
                          (((x) & 0x00ff0000) >>  8) | (((x) & 0xff000000) >> 24) )
#define SWAP16(x)	 (x)
#define SWAP32(x)	 (x)
#define SHORTSWAP32(x)	 (x)

void logo_plot(void *screen, int width, int x, int y)
{

	int xcount, i;
	int skip   = (width - VIDEO_LOGO_WIDTH) * VIDEO_PIXEL_SIZE;
	int ycount = VIDEO_LOGO_HEIGHT;
	unsigned char r, g, b, *logo_red, *logo_blue, *logo_green;
	unsigned char *source;
	unsigned char *dest = (unsigned char *)screen + ((y * width * VIDEO_PIXEL_SIZE) + x);

#ifdef CONFIG_VIDEO_BMP_LOGO
	source = bmp_logo_bitmap;

	/* Allocate temporary space for computing colormap			 */
	logo_red = malloc (BMP_LOGO_COLORS);
	logo_green = malloc (BMP_LOGO_COLORS);
	logo_blue = malloc (BMP_LOGO_COLORS);
	/* Compute color map							 */
	for (i = 0; i < VIDEO_LOGO_COLORS; i++) {
		logo_red[i] = (bmp_logo_palette[i] & 0x0f00) >> 4;
		logo_green[i] = (bmp_logo_palette[i] & 0x00f0);
		logo_blue[i] = (bmp_logo_palette[i] & 0x000f) << 4;
	}
#else
	source = linux_logo;
	logo_red = linux_logo_red;
	logo_green = linux_logo_green;
	logo_blue = linux_logo_blue;
#endif

	if (VIDEO_DATA_FORMAT == GDF__8BIT_INDEX) {
#if 0
		for (i = 0; i < VIDEO_LOGO_COLORS; i++) {
			video_set_lut (i + VIDEO_LOGO_LUT_OFFSET,
				       logo_red[i], logo_green[i], logo_blue[i]);
		}
#endif
	}

	while (ycount--) {
		xcount = VIDEO_LOGO_WIDTH;
		while (xcount--) {
			r = logo_red[*source - VIDEO_LOGO_LUT_OFFSET];
			g = logo_green[*source - VIDEO_LOGO_LUT_OFFSET];
			b = logo_blue[*source - VIDEO_LOGO_LUT_OFFSET];

			switch (VIDEO_DATA_FORMAT) {
			case GDF__8BIT_INDEX:
				*dest = *source;
				break;
			case GDF__8BIT_332RGB:
				*dest = ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
				break;
			case GDF_12BIT_444RGB:
				*(unsigned short *) dest =
					SWAP16 ((unsigned short) (((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)));
				break;
			case GDF_15BIT_555RGB:
				*(unsigned short *) dest =
					SWAP16 ((unsigned short) (((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)));
				break;
			case GDF_16BIT_565RGB:
				*(unsigned short *) dest =
					SWAP16 ((unsigned short) (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)));
				break;
			case GDF_32BIT_X888RGB:
				*(unsigned long *) dest =
					SWAP32 ((unsigned long) ((r << 16) | (g << 8) | b));
				break;
			case GDF_24BIT_888RGB:
#ifdef VIDEO_FB_LITTLE_ENDIAN
				dest[0] = b;
				dest[1] = g;
				dest[2] = r;
#else
				dest[0] = r;
				dest[1] = g;
				dest[2] = b;
#endif
				break;
			}
			source++;
			dest += VIDEO_PIXEL_SIZE;
		}
		dest += skip;
	}
#ifdef CONFIG_VIDEO_BMP_LOGO
	free (logo_red);
	free (logo_green);
	free (logo_blue);
#endif
}

static void *video_logo(void)
{
#ifdef CONFIG_SPLASH_SCREEN//no
	char *s;
	ulong addr;
	if ((s = getenv ("splashimage")) != NULL) {
//		addr = simple_strtoul (s, NULL, 16);
		addr = NULL;
		if (video_display_bitmap (addr, 0, 0) == 0) {
			return ((void *) (video_fb_address));
		}
	}
#endif /* CONFIG_SPLASH_SCREEN */

	logo_plot(video_fb_address, VIDEO_COLS, (VIDEO_COLS - VIDEO_LOGO_WIDTH)* VIDEO_PIXEL_SIZE, 0);

	//video_drawstring (VIDEO_INFO_X, VIDEO_INFO_Y, (unsigned char *)info);

#ifdef CONFIG_CONSOLE_EXTRA_INFO//no
	{
		char info[128] = " PMON on Loongson-ICT2E,build r31,2007.8.24.";
		int i, n = ((VIDEO_LOGO_HEIGHT - VIDEO_FONT_HEIGHT) / VIDEO_FONT_HEIGHT);

		for (i = 1; i < n; i++) {
			video_get_info_str (i, info);
			if (*info)
				video_drawstring (VIDEO_INFO_X,
						  VIDEO_INFO_Y + i * VIDEO_FONT_HEIGHT,
						  (unsigned char *)info);
		}
	}
#endif

	return (video_fb_address + VIDEO_LOGO_HEIGHT * VIDEO_LINE_LEN);
}


int fb_init (unsigned long fbbase,unsigned long iobase)
{
	unsigned char color8;

    int i,j;
	pGD = &GD;
	
#if defined(VGA_NOTEBOOK_V1)
	pGD->winSizeX  = 1280;
	pGD->winSizeY  = 800;
#elif defined(VGA_NOTEBOOK_V2)
	pGD->winSizeX  = 1024;
	pGD->winSizeY  = 768;
#else//yes
	pGD->winSizeX  = 640;
	pGD->winSizeY  = 480;
#endif

#if defined(X800x600)
	pGD->winSizeX  = 800;
	pGD->winSizeY  = 600;
#elif defined(X1440x900)
	pGD->winSizeX  = 1440;
	pGD->winSizeY  = 900;
#elif defined(X1024x768)
	pGD->winSizeX  = 1024;
	pGD->winSizeY  = 768;
#elif defined(X1280x1024)
	pGD->winSizeX  = 1280;
	pGD->winSizeY  = 1024;
#elif defined(X800x480)
	pGD->winSizeX  = 800;
	pGD->winSizeY  = 480;
#elif defined(X480x640)
	pGD->winSizeX  = 480;
	pGD->winSizeY  = 640;
#elif defined(X480x272)//yes
	pGD->winSizeX  = 480;
	pGD->winSizeY  = 272;
#elif defined(X320x240)
	pGD->winSizeX  = 320;
	pGD->winSizeY  = 240;
#elif defined(X240x320)
	pGD->winSizeX  = 240;
	pGD->winSizeY  = 320;
#else
	pGD->winSizeX  = FB_XSIZE;
	pGD->winSizeY  = FB_YSIZE;
#endif	


#ifdef CONFIG_FB_DYN//yes
	pGD->winSizeX  = getenv("xres")? strtoul(getenv("xres"),0,0):FB_XSIZE;
	pGD->winSizeY  = getenv("yres")? strtoul(getenv("yres"),0,0):FB_YSIZE;
#endif
	
#if defined(CONFIG_VIDEO_1BPP)
	pGD->gdfIndex  = GDF__1BIT;
	pGD->gdfBytesPP= 1;
#elif defined(CONFIG_VIDEO_2BPP)
	pGD->gdfIndex  = GDF__2BIT;
	pGD->gdfBytesPP= 1;
#elif defined(CONFIG_VIDEO_4BPP)
	pGD->gdfIndex  = GDF__4BIT;
	pGD->gdfBytesPP= 1;
#elif defined(CONFIG_VIDEO_8BPP_INDEX)
	pGD->gdfBytesPP= 1;
	pGD->gdfIndex  = GDF__8BIT_INDEX;
#elif defined(CONFIG_VIDEO_8BPP)
	pGD->gdfBytesPP= 1;
	pGD->gdfIndex  = GDF__8BIT_332RGB;
#elif defined(CONFIG_VIDEO_12BPP)
	pGD->gdfBytesPP= 2;
	pGD->gdfIndex  = GDF_12BIT_444RGB;
#elif defined(CONFIG_VIDEO_15BPP)
	pGD->gdfBytesPP= 2;
	pGD->gdfIndex  = GDF_15BIT_555RGB;
#elif defined(CONFIG_VIDEO_16BPP)
	pGD->gdfBytesPP= 2;
	pGD->gdfIndex  = GDF_16BIT_565RGB;
#elif defined(CONFIG_VIDE0_24BPP)
	pGD->gdfBytesPP= 3;
	pGD->gdfIndex=GDF_24BIT_888RGB;
#elif defined(CONFIG_VIDEO_32BPP)
	pGD->gdfBytesPP= 4;
	pGD->gdfIndex  = GDF_32BIT_X888RGB;
#else
	pGD->gdfBytesPP= 2;
	pGD->gdfIndex  = GDF_16BIT_565RGB;
#endif
	if (fbbase<0x20000000)
		pGD->frameAdrs = 0xb0000000 | fbbase;
	else
		pGD->frameAdrs = fbbase;

	printf("cfb_console init, fb=0x%x\n", pGD->frameAdrs);

	video_fb_address = (void *) VIDEO_FB_ADRS;
#ifdef CONFIG_VIDEO_HW_CURSOR
	video_init_hw_cursor (VIDEO_FONT_WIDTH, VIDEO_FONT_HEIGHT);
#endif

	/* Init drawing pats */
	switch (VIDEO_DATA_FORMAT) {
#if 1
	case GDF__8BIT_INDEX:
		video_set_lut(0x01, CONSOLE_FG_COL, CONSOLE_FG_COL, CONSOLE_FG_COL);
		video_set_lut(0x00, CONSOLE_BG_COL, CONSOLE_BG_COL, CONSOLE_BG_COL);
		fgx = 0x01010101;
		bgx = 0x00000000;
	break;
#endif
	case GDF__1BIT:
		fgx=1;
		bgx=0;
	break;
	case GDF__2BIT:
		fgx=3;
		bgx=0;
	break;
	case GDF__4BIT:
		fgx=0xf;
		bgx=0;
	break;
	case GDF__8BIT_332RGB:
		color8 = ((CONSOLE_FG_COL & 0xe0) |
				((CONSOLE_FG_COL >> 3) & 0x1c) | CONSOLE_FG_COL >> 6);
		fgx = (color8 << 24) | (color8 << 16) | (color8 << 8) | color8;
		color8 = ((CONSOLE_BG_COL & 0xe0) |
				((CONSOLE_BG_COL >> 3) & 0x1c) | CONSOLE_BG_COL >> 6);
		bgx = (color8 << 24) | (color8 << 16) | (color8 << 8) | color8;
	break;
	case GDF_12BIT_444RGB:
		fgx = (((CONSOLE_FG_COL >> 4) << 28) |
		        ((CONSOLE_FG_COL >> 4) << 24) | ((CONSOLE_FG_COL >> 4) << 20) |
				((CONSOLE_FG_COL >> 4) << 16) | ((CONSOLE_FG_COL >> 4) << 12) |
				((CONSOLE_FG_COL >> 4) << 8) | ((CONSOLE_FG_COL >> 4) << 4) |
				(CONSOLE_FG_COL >> 4));
		bgx = (((CONSOLE_BG_COL >> 4) << 28) |
                ((CONSOLE_BG_COL >> 4) << 24) | ((CONSOLE_BG_COL >> 4) << 20) |
				((CONSOLE_BG_COL >> 4) << 16) | ((CONSOLE_BG_COL >> 4) << 12) |
				((CONSOLE_BG_COL >> 4) << 8) | ((CONSOLE_BG_COL >> 4) << 4) |
				(CONSOLE_BG_COL >> 4));
	break;
	case GDF_15BIT_555RGB:
		fgx = (((CONSOLE_FG_COL >> 3) << 26) |
				((CONSOLE_FG_COL >> 3) << 21) | ((CONSOLE_FG_COL >> 3) << 16) |
				((CONSOLE_FG_COL >> 3) << 10) | ((CONSOLE_FG_COL >> 3) << 5) |
				(CONSOLE_FG_COL >> 3));
		bgx = (((CONSOLE_BG_COL >> 3) << 26) |
				((CONSOLE_BG_COL >> 3) << 21) | ((CONSOLE_BG_COL >> 3) << 16) |
				((CONSOLE_BG_COL >> 3) << 10) | ((CONSOLE_BG_COL >> 3) << 5) |
				(CONSOLE_BG_COL >> 3));
	break;
	case GDF_16BIT_565RGB:
		fgx = (((CONSOLE_FG_COL >> 3) << 27) |
				((CONSOLE_FG_COL >> 2) << 21) | ((CONSOLE_FG_COL >> 3) << 16) |
				((CONSOLE_FG_COL >> 3) << 11) | ((CONSOLE_FG_COL >> 2) << 5) |
				(CONSOLE_FG_COL >> 3));
		bgx = (((CONSOLE_BG_COL >> 3) << 27) |
				((CONSOLE_BG_COL >> 2) << 21) | ((CONSOLE_BG_COL >> 3) << 16) |
				((CONSOLE_BG_COL >> 3) << 11) | ((CONSOLE_BG_COL >> 2) << 5) |
				(CONSOLE_BG_COL >> 3));
	break;
	case GDF_32BIT_X888RGB:
		fgx = (CONSOLE_FG_COL << 16) | (CONSOLE_FG_COL << 8) | CONSOLE_FG_COL;
		bgx = (CONSOLE_BG_COL << 16) | (CONSOLE_BG_COL << 8) | CONSOLE_BG_COL;
	break;
	case GDF_24BIT_888RGB:
		fgx = (CONSOLE_FG_COL << 24) | (CONSOLE_FG_COL << 16) |
			(CONSOLE_FG_COL << 8) | CONSOLE_FG_COL;
		bgx = (CONSOLE_BG_COL << 24) | (CONSOLE_BG_COL << 16) |
			(CONSOLE_BG_COL << 8) | CONSOLE_BG_COL;
	break;
	}
	eorx = fgx ^ bgx;

//	memsetl(video_fb_address, VIDEO_COLS * VIDEO_ROWS * VIDEO_PIXEL_SIZE / 4, CONSOLE_BG_COL);
#ifdef CONFIG_VIDEO_LOGO//yes
	/* Plot the logo and get start point of console */
	video_console_address = video_logo();
#else
	video_console_address = video_fb_address;
#endif

	/* Initialize the console */
	console_col = 0;
	console_row = 0;

//	memset(console_buffer, ' ', sizeof console_buffer);
#if defined(MEM_PRINTTO_VIDEO)//yes
	memfb = malloc(CONSOLE_ROWS * CONSOLE_COLS);
//	memset(memfb, 0, CONSOLE_ROWS * CONSOLE_COLS);
#endif
	return 0;
}













static int config_fb(unsigned int base)
{
	int i, mode = -1;
	unsigned int val;

	for (i=0; i<sizeof(vgamode)/sizeof(struct vga_struc); i++) 
	{
		if (vgamode[i].hr == fb_xsize && vgamode[i].vr == fb_ysize) 
		{
			mode = i;
		#if defined(LS1ASOC)
		{
			int out;
			out = caclulatefreq(APB_CLK/1000, vgamode[mode].pclk);
			/*output pix1 clock  pll ctrl */
			#ifdef DC_FB0
			*(volatile int *)0xbfd00410 = out;
			delay(1000);
			#endif
			/*output pix2 clock pll ctrl */
			#ifdef DC_FB1
			*(volatile int *)0xbfd00424 = out;
			delay(1000);
			#endif
			/*inner gpu dc logic fifo pll ctrl,must large then outclk*/
			out = caclulatefreq(APB_CLK/1000, 200000);
			*(volatile int *)0xbfd00414 = out;
			delay(1000);
		}
		#elif defined(LS1BSOC)
		#ifdef CONFIG_VGA_MODEM	/* 只用于1B的外接VGA */
		{
			struct ls1b_vga *input_vga;

			for (input_vga=ls1b_vga_modes; input_vga->ls1b_pll_freq!=0; ++input_vga) {
		//		if((input_vga->xres == fb_xsize) && (input_vga->yres == fb_ysize) && 
		//			(input_vga->refresh == frame_rate)) {
				if ((input_vga->xres == fb_xsize) && (input_vga->yres == fb_ysize)) {
					break;
				}
			}
			if (input_vga->ls1b_pll_freq) {
				writel(input_vga->ls1b_pll_freq, LS1X_CLK_PLL_FREQ);
				writel(input_vga->ls1b_pll_div, LS1X_CLK_PLL_DIV);
				delay(100);
				caclulatefreq(input_vga->ls1b_pll_freq, input_vga->ls1b_pll_div);
			}
		}
		#else
		{
			u32 divider_int, div_reg;

			divider_int = tgt_pllfreq() / (vgamode[mode].pclk * 1000) / 4;
			/* check whether divisor is too small. */
			if (divider_int < 1) {
				printf("Warning: clock source is too slow."
						"Try smaller resolution\n");
				divider_int = 1;
			}
			else if (divider_int > 15) {
				printf("Warning: clock source is too fast."
						"Try smaller resolution\n");
				divider_int = 15;
			}
			/* Set setting to reg. */
			div_reg = readl(LS1X_CLK_PLL_DIV);
			div_reg |= 0x00003000;	//dc_bypass 置1
			div_reg &= ~0x00000030;	//dc_rst 置0
			div_reg &= ~(0x1f<<26);	//dc_div 清零
			div_reg |= divider_int << 26;
			writel(div_reg, LS1X_CLK_PLL_DIV);
			div_reg &= ~0x00001000;	//dc_bypass 置0
			writel(div_reg, LS1X_CLK_PLL_DIV);
		}
		#endif //CONFIG_VGA_MODEM
		#elif defined(LS1CSOC)
		{
			unsigned int lcd_div, div_reg;

			lcd_div = tgt_pllfreq() / (vgamode[mode].pclk * 1000);
			div_reg = readl(LS1X_CLK_PLL_DIV);
			/* 注意：首先需要把分频使能位清零 */
			writel(div_reg & ~DIV_DC_EN, LS1X_CLK_PLL_DIV);
			div_reg |= DIV_DC_EN | DIV_DC_SEL_EN | DIV_DC_SEL;
			div_reg &= ~DIV_DC;
			div_reg |= lcd_div << DIV_DC_SHIFT;
			writel(div_reg, LS1X_CLK_PLL_DIV);
		}
		#endif //#ifdef LS1ASOC
		break;
		}
	}

	if (mode < 0) {
		printf("\n\n\nunsupported framebuffer resolution,choose from bellow:\n");
		for (i=0; i<sizeof(vgamode)/sizeof(struct vga_struc); i++)
			printf("%dx%d, ", vgamode[i].hr, vgamode[i].vr);
		printf("\n");
		return -1;
	}

	/* Disable the panel 0 */
	writel((unsigned int)mem_ptr & 0x0fffffff, base+OF_BUF_ADDR0);
	writel((unsigned int)mem_ptr & 0x0fffffff, base+OF_BUF_ADDR1);
//	writel(0x00000000, base+OF_DITHER_CONFIG);
//	writel(0x00000000, base+OF_DITHER_TABLE_LOW);
//	writel(0x00000000, base+OF_DITHER_TABLE_HIGH);
	/* PAN_CONFIG寄存器最高位需要置1，否则lcd时钟延时一段时间才会有输出 */
	#ifdef CONFIG_VGA_MODEM
	writel(0x80001311, base+OF_PAN_CONFIG);
	#else
	writel(0x80001111 | vgamode[mode].pan_config, base+OF_PAN_CONFIG);
	#endif
//	writel(0x00000000, base+OF_PAN_TIMING);

	writel((vgamode[mode].hfl<<16) | vgamode[mode].hr, base + OF_HDISPLAY);
	writel(vgamode[mode].hvsync_polarity | (vgamode[mode].hse<<16) | vgamode[mode].hss, base + OF_HSYNC);
	writel((vgamode[mode].vfl<<16) | vgamode[mode].vr, base + OF_VDISPLAY);
	writel(vgamode[mode].hvsync_polarity | (vgamode[mode].vse<<16) | vgamode[mode].vss, base + OF_VSYNC);

#if defined(CONFIG_VIDEO_32BPP)
	writel(0x00100104, base+OF_BUF_CONFIG);
	writel((fb_xsize*4+BURST_SIZE)&~BURST_SIZE, base+OF_BUF_STRIDE);
	#ifdef LS1BSOC
	*(volatile int *)0xbfd00420 &= ~0x08;
	*(volatile int *)0xbfd00420 |= 0x05;
	#endif
#elif defined(CONFIG_VIDEO_24BPP)
	writel(0x00100104, base+OF_BUF_CONFIG);
	writel((fb_xsize*3+BURST_SIZE)&~BURST_SIZE, base+OF_BUF_STRIDE);
	#ifdef LS1BSOC
	*(volatile int *)0xbfd00420 &= ~0x08;
	*(volatile int *)0xbfd00420 |= 0x05;
	#endif
#elif defined(CONFIG_VIDEO_16BPP)
	writel(0x00100103, base+OF_BUF_CONFIG);
	writel((fb_xsize*2+BURST_SIZE)&~BURST_SIZE, base+OF_BUF_STRIDE);
	#ifdef LS1BSOC
	*(volatile int *)0xbfd00420 &= ~0x07;
	#endif
#elif defined(CONFIG_VIDEO_15BPP)
	writel(0x00100102, base+OF_BUF_CONFIG);
	writel((fb_xsize*2+BURST_SIZE)&~BURST_SIZE, base+OF_BUF_STRIDE);
	#ifdef LS1BSOC
	*(volatile int *)0xbfd00420 &= ~0x07;
	#endif
#elif defined(CONFIG_VIDEO_12BPP)
	writel(0x00100101, base+OF_BUF_CONFIG);
	writel((fb_xsize*2+BURST_SIZE)&~BURST_SIZE, base+OF_BUF_STRIDE);
	#ifdef LS1BSOC
	*(volatile int *)0xbfd00420 &= ~0x07;
	#endif
#else	/* 16bpp */
	writel(0x00100103, base+OF_BUF_CONFIG);
	writel((fb_xsize*2+BURST_SIZE)&~BURST_SIZE, base+OF_BUF_STRIDE);
	#ifdef LS1BSOC
	*(volatile int *)0xbfd00420 &= ~0x07;
	#endif
#endif	//#if defined(CONFIG_VIDEO_32BPP)
	writel(0, base+OF_BUF_ORIG);

	{	/* 显示数据输出使能 */
		int timeout = 204800;
		val = readl((base+OF_BUF_CONFIG));
		do {
			writel(val|0x100, base+OF_BUF_CONFIG);
			val = readl(base+OF_BUF_CONFIG);
		} while (((val & 0x100) == 0) && (timeout-- > 0));
	}
	return 0;
}

unsigned long dc_doinit(void)
{
	unsigned int smem_len;
	/* Driver IC需要在ls1x lcd控制器初始化前现进行初始化，否则有可能出现没显示的现象 */
#if defined(CONFIG_JBT6K74)
//	printf("JBT6K74 TFT LCD Driver IC\n");
	jbt6k74_init();
#endif
#if defined(CONFIG_ILI9341)
//	printf("ILI9341 TFT LCD Driver IC\n");
	ili9341_hw_init();
#endif
#if defined(CONFIG_NT35310)
//	printf("NT35310 TFT LCD Driver IC\n");
	nt35310_init();
#endif
#if defined(CONFIG_ST7565)
//	printf("ST7565 LCD Driver IC\n");
	st7565_init();
#endif

	fb_xsize  = getenv("xres") ? strtoul(getenv("xres"),0,0) : FB_XSIZE;
	fb_ysize  = getenv("yres") ? strtoul(getenv("yres"),0,0) : FB_YSIZE;
	frame_rate  = getenv("frame_rate") ? strtoul(getenv("frame_rate"),0,0) : 60;

	smem_len = PAGE_ALIGN(fb_xsize * fb_ysize * 4);
	mem_ptr = (char *)malloc(smem_len); /* 高位地在开始分配内存 */
	if (!mem_ptr) 
	{
		printf("Unable to allocate memory for lcd.\n");
		return -ENOMEM;
	}
	memset(mem_ptr, 0, smem_len);
	mem_ptr = (char *)((unsigned int)mem_ptr | 0xa0000000); /* 需要转换为umap ucache的地址 */
	fb_addr = (unsigned long)mem_ptr;

#ifdef DC_FB0
	config_fb(DC_BASE_ADDR0);
#endif
#ifdef DC_FB1
	config_fb(DC_BASE_ADDR1);
#endif
#ifdef DC_CURSOR
	config_cursor();
#endif

	return (unsigned long)mem_ptr;
}


#define SHUT_CTRL 0xbfd00420
void dc_init(void)
{
	if(0)
	{
		unsigned int shut_ctrl_value=readl(SHUT_CTRL);
		printf("in dc_init shut_ctrl_value:0x%08x\n",shut_ctrl_value);
		writel(shut_ctrl_value|0x1, SHUT_CTRL);
		shut_ctrl_value=readl(SHUT_CTRL);
		printf("in dc_init shut_ctrl_value:0x%08x\n",shut_ctrl_value);
		return;
	}
	/* 打开LCD背光 */
	*(volatile unsigned int *)0xbfd010c0 |= (1 << 6);
	*(volatile unsigned int *)0xbfd010d0 &= ~(1 << 6);
	*(volatile unsigned int *)0xbfd010f0 |= (1 << 6);
	unsigned long fbaddress;
	fbaddress = dc_doinit();
	fbaddress |= 0xa0000000;
	
	fb_init(fbaddress, 0);
}
