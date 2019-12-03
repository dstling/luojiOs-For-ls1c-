#ifndef BUILDCONFIG_H
#define BUILDCONFIG_H
//!!!!重要
//编译完此bin后，确认下有没有大于这个值，大了务必调整
//ROM空间布局（详见env.c）
//bootRomSize=256k NVRAM_SECSIZE=4k时rom空间分配
//rom空间分配
//---------------------------------------------------------------------------------------------------------------------------
//|	名称	|		luojios.bin位置			|		set配置参数保存位置				|					空余空间							|
//---------------------------------------------------------------------------------------------------------------------------
//|	地址	|		256*1024				|		256*1024-4*1024			|					空余空间							|
//---------------------------------------------------------------------------------------------------------------------------
#define NVRAM_SECSIZE		(4*1024) 	//环境变量空间 4k=4096byte 默认包含在BOOT_ROM_SIZE kB的空间尾部 之所以是4k，是为了环境变量参数重写入flash 最小4k擦除 免得破坏其他数据
#define BOOT_ROM_SIZE 		(512-4*10) 		//单位KB 被4整除即可 编译完此bin后，确认下有没有大于这个值，大了务必调整重新编译

#define BUILD_TEST_FUN 0//编译testFun.c内的函数
#define LS1CSOC
#define DC_FB0
#define MTDPARTS	//定义环境变量MTD分区识别
#define CPU_NAME "\"Loongson 1C300 OpenLoongson\""
#define whatcpu 0

#define LOCAL_IP		"192.168.11.3"
#define LOCAL_MASK		"255.255.255.0"
#define LOCAL_GATEWY	"192.168.11.1"
#define LOCAL_MAC		"00:55:7B:B5:7D:F7"

#define	ENV_TFTP_SERVER_IP	"192.168.11.2"	//tftp 服务器ip
#define	ENV_TFTP_PMON_FILE	"luojios.bin"	//默认flash的文件名

#define	LOAD_ELF_ADDR_FILE	"tftp://192.168.11.2/rtthread.elf"
#define	LOAD_ELF_ADDR_FILE2	"/dev/mtd0"
//#define	LOAD_ELF_ADDR_FILE	"/dev/fat@usb0/rtthread.elf"
#define PLL_REG0_NAME	"pll_reg0"
#define PLL_REG1_NAME	"pll_reg1"

#define TCPIP_THREAD_PRIO_GLO 12//tcpip线程优先级 使用到网络连接的线程优先级不要小于此值 15以上吧

//#define DebugPR printf("loadelfDebug info %s %s:%d\n",__FILE__ , __func__, __LINE__)
//#define	USB_PRINTF(fmt,args...)	printf (fmt ,##args)


//下述有待测试 load linux测试用 
#define	_JBLEN	83		/* size, in longs, of a jmp_buf */
typedef long jmp_buf[_JBLEN] __attribute__((aligned(8)));

/*
#define fat_getChain(a,b,c)	({int ret=fat_getChain2(a,b,c);\
								printf("%s,%d,\n",__func__, __LINE__);\
								ret;\
								})
//*/

#endif
