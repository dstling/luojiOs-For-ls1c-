#include <buildconfig.h>
#include <types.h>
#include <stdio.h>
#include <shell.h>
#include <env.h>
#include <ls1c_regs.h>
#define readl(addr) (*(volatile unsigned long *)(addr))

#define NVRAM_OFFS			0							//参数保存位置偏移 这里不偏移
//#define NVRAM_SECSIZE		(4*1024) 					//环境变量空间 4k=4096byte 默认包含在256kB的空间尾部 之所以是4k，是为了环境变量参数重写入flash 最小4k擦除 免得破坏其他数据
//#define BOOT_ROM_SIZE 		256 						//单位KB 被4整除即可 编译完此bin后，确认下有没有大于这个值，大了务必调整
int 	bootRomSize		=	(BOOT_ROM_SIZE*1024);		//boot rom 存储在flash的前bootRomSize内 必须4k对齐

#define NVRAM_POS			(bootRomSize-NVRAM_SECSIZE)	//该地址之前的空间用于存储PMON程序 

static int nvram_invalid =	0;							//配置参数可用标志
#define NVAR				64							//配置参数的最大数目
struct envpair {
    char	*name;
    char	*value;
};

struct envpair envvar[NVAR];
static int	envinited = 0;//环境变量初始化标志


struct envpair stdenvtab[]={
	{ENV_IP_NAME,		LOCAL_IP},
	{ENV_MASK_NAME,		LOCAL_MASK},
	{ENV_GATEWAY_NAME,	LOCAL_GATEWY},
	{ENV_MAC_NAME,		LOCAL_MAC},
		
	{ENV_TFTP_SERVER_IP_NAME,	ENV_TFTP_SERVER_IP},
	{ENV_TFTP_PMON_FILE_NAME,	ENV_TFTP_PMON_FILE},

	{"mtdparts", "ls1x-nand:14M@0(kernel),100M@14M(rootfs),14M@114M(data);spi-flash:512K@0(pmon),4K@520192(nvram)"},

	//下面这些参数在引导linux内核的时候可能有用，在g时传入内核
	{"memsize", "32"},
	{"highmemsize", "0"},
	{PLL_REG0_NAME, "0x8000549c"},
	{PLL_REG1_NAME, "0x00008283"},
	{"cpuclock", "252000000"},
	{"busclock", "126000000"},
	{"xres", "480"},
	{"yres", "272"},
	{"depth", "16"},
	{"systype", "FCR"},
	{"moresz", "10"},
	{"brkcmd", "l -r @cpc 1"},
	{"TZ", "UTC8"},
		
	{"datasize", "-b"},

	{"dlecho", "off"},
	{"dlproto", "none"},
	{"bootp", "no"},
	{"hostport", "tty0"},
	{"inalpha", "hex"},
	{"inbase", "16"},
	{"regstyle", "sw"},
	{"rptcmd", "trace"},
	{"trabort", "^K"},
	{"ulcr", "cr"},
	{"uleof", "%"},
	{"showsym", "yes"},
	{"fpfmt", "both"},
	{"fpdis", "yes"},
	{"bootdelay", "8"},

	
	//{"loadelfAddr",	LOAD_ELF_ADDR_FILE},
	{"loadelfAddr", LOAD_ELF_ADDR_FILE2},
	{"append", "console=ttyS2,115200 root=/dev/mtdblock1 rw rootfstype=yaffs2 video=ls1bfb:vga1024x768-24@60 panic=10 noswap"},
	//{"append", "console=ttyS2,115200 root=/dev/mtdblock1 rw rootfstype=yaffs2 video=ls1bfb:vga480x272-24@60 panic=10 noswap "},//lpj=501248
	//{"append", "console=ttyS2,115200 root=/dev/mtdblock1 rw rootfstype=yaffs2 video=ls1bfb:vga320x240-16@60 panic=10 noswap"},
	//{"append", "console=ttyS2,115200 root=/dev/mtdblock1 rw rootfstype=yaffs2  panic=10 noswap"},
	//{"",""},
	//{"",""},
	{"al", "mtd0"},
	{0}
};

void envsize (int *ec, int *slen)
{
	struct envpair *ep;

	*ec = 0;
	*slen = 0;
	for (ep = envvar; ep < &envvar[NVAR]; ep++) 
	{
		if (ep->name) 
		{
			*ec += 1;
			*slen += strlen (ep->name) + 2; /* = and NULL */
			if (ep->value) {
				*slen += strlen(ep->value);
			}
		}
	}
}
void envbuild (char **vsp, char *ssp)
{
	struct envpair *ep;

	for(ep = envvar; ep < &envvar[NVAR]; ep++) 
	{
		if (ep->name) 
		{
			*vsp++ = ssp;
			ssp += sprintf (ssp, "%s=", ep->name) + 1;
			if (ep->value) 
			{
				ssp += sprintf(ssp - 1, "%s", ep->value);
			}
		}
	}
	*vsp++ = (char *)0;
}

char *getenv (const char *name)
{
    if (envinited) //已经初始化了
	{
		struct envpair *ep;
		for (ep = envvar; ep < &envvar[NVAR]; ep++)
		  if (ep->name &&striequ (name, ep->name))
		    return ep->value ? ep->value : "";
    } 
	else 
	    return 0;
}

//set 					dst 		123
/*
int do_setenv (char *name, char*value)
{
	if (_setenv (name, value)) 
	{
		const struct stdenv *sp;
		if ((sp = getstdenv (name)) && striequ (value, sp->init)) 
		{
			// set to default: remove from non-volatile ram 
			return tgt_unsetenv (name);
		}
		else if(!temp) 
		{
			// new value: save in non-volatile ram 
			return tgt_setenv (name, value);
		}
		else {
			return(1);
		}
	}
	return 0;
}
*/

static int set_mem_env(char *name, char *value)//更新内存中的环境变量envvar
{
    struct envpair *ep;
    struct envpair *bp = 0;
    const struct stdenv *sp;

    for (ep = envvar; ep < &envvar[NVAR]; ep++) 
	{
		if (!ep->name && !bp)
		  bp = ep;//bp是最后一个内存中为空的环境变量指针
		else if (ep->name && striequ (name, ep->name))
		  break;//如果找到 就定位了 如果找不到 就是最后一个位置
    }
    
    if (ep < &envvar[NVAR]) 
	{
		/* must have got a match, free old value */
		if (ep->value) 
		{
		    free (ep->value); 
			ep->value = 0;
		}
    } 
	else if (bp) 
	{
		/* new entry */
		ep = bp;
		ep->name = malloc (strlen (name) + 1);
		if (!ep->name)
		  return 0;
		strcpy (ep->name, name);
    } 
	else 
	{
		return 0;
    }

    if (value) 
	{
		ep->value = malloc (strlen (value) + 1);
		if (!ep->value) 
		{
		    free (ep->name); ep->name = 0;
		    return 0;
		}
		strcpy (ep->value, value);
    }

    return 1;
}


static int cksum(void *p, size_t s, int set)
{
	u_int16_t sum = 0;
	u_int8_t *sp = p;
	int sz = s / 2;

	if (set) 
	{
		*sp = 0;	/* Clear checksum */
		*(sp+1) = 0;	/* Clear checksum */
	}
	while (sz--) 
	{
		sum += (*sp++) << 8;
		sum += *sp++;
	}
	if (set) 
	{
		sum = -sum;
		*(u_int8_t *)p = sum >> 8;
		*((u_int8_t *)p+1) = sum;
	}
	return(sum);
}

void nvram_get(char *buffer)
{
	spi_flash_read_area(NVRAM_POS, buffer, NVRAM_SECSIZE);//NVRAM_SECSIZE
}

void nvram_put(char *buffer)
{
	//spi_flash_erase_area(NVRAM_POS, NVRAM_POS+NVRAM_SECSIZE, 0x10000);
	spi_flash_erase_sector(NVRAM_POS, NVRAM_POS+NVRAM_SECSIZE, 4096);//NVRAM_SECSIZE 
	spi_flash_write_area(NVRAM_POS, buffer, NVRAM_SECSIZE);//NVRAM_SECSIZE
}

void resetNorEnvSpace(void)
{
	char *nvramsecbuf,*nvrambuf;
	if(nvram_invalid) 
	{
		nvramsecbuf = (char *)malloc(NVRAM_SECSIZE);
		if(nvramsecbuf == 0) 
		{
			printf("Warning! Unable to malloc nvrambuffer!\n");
			return(-1);
		}
		nvrambuf = nvramsecbuf + (NVRAM_OFFS & (NVRAM_SECSIZE - 1));
		memset(nvrambuf, -1,NVRAM_SECSIZE);//NVRAM_SPECIAL_SIZE
		nvrambuf[2] = '\0';
		nvrambuf[3] = '\0';
		cksum((void *)nvrambuf,NVRAM_SECSIZE, 1);//NVRAM_SPECIAL_SIZE
		printf("Warning! Checksum Env datas fail in norFlash. Reset at norFlash Adddress:0X%08X\n",bootRomSize-NVRAM_SECSIZE);
		printf("Wait a minute...\n");
		//show_hex(nvramsecbuf,NVRAM_SECSIZE,0);
		nvram_put(nvramsecbuf);
		/*
		char buffs[66]="";
		read_flash_data(NVRAM_POS,64,buffs);
		show_hex(buffs,64,NVRAM_POS);
		read_flash_data(NVRAM_POS+NVRAM_SPECIAL_SIZE,32,buffs);
		show_hex(buffs,64,NVRAM_POS+NVRAM_SPECIAL_SIZE);
		//*/
		nvram_invalid = 0;
		free(nvramsecbuf);
	}
}


int unset_rom_env(char *name)//清除已经存在的name参数
{
	char *ep, *np, *sp;
	char *nvram;
	char *nvrambuf;
	char *nvramsecbuf;
	int status = 0;
	
	if(nvram_invalid) 
		return(0);

	nvramsecbuf = nvrambuf = nvram = (char *)malloc(NVRAM_SECSIZE);
	if(nvramsecbuf==NULL)
	{
		printf("error info %s:%d\n", __func__, __LINE__);
		return 0;
	}
	nvram_get(nvram);
	ep = nvrambuf + 2;
	
	while ((*ep != '\0') && (ep < nvrambuf +NVRAM_SECSIZE )) //NVRAM_SPECIAL_SIZE
	{
		np = name;
		sp = ep;

		while ((*ep == *np) && (*ep != '=') && (*np != '\0')) //查找有没有和name一样的参数
		{
			ep++;
			np++;
		}
		if ((*np == '\0') && ((*ep == '\0') || (*ep == '='))) 
		{
			while (*ep++);
			while (ep < nvrambuf + NVRAM_SECSIZE) //
			{
				*sp++ = *ep++;
			}
			if(nvrambuf[2] == '\0') 
			{
				nvrambuf[3] = '\0';
			}
			cksum(nvrambuf, NVRAM_SECSIZE, 1);//NVRAM_SPECIAL_SIZE修改了 重新计算
			nvram_put(nvram);
			status = 1;//找到了
			break;
		}
		else if (*ep != '\0') 
		{
			while (*ep++ != '\0');
		}
	}

	free(nvramsecbuf);
	return(status);
	
}


int set_rom_env(char *name, char *value)//更新rom中的环境变量设置
{
	char *ep;
	int envlen;//新加入的变量和value总长度
	char *nvrambuf;
	char *nvramsecbuf;

	//printf("Setting env datas into norFlash,name:%s,value:%s.\n",name,value);
	// Calculate total env mem size requiered 
	envlen = strlen(name);
	if(envlen == 0) 
		return(0);
	if(value != NULL) 
		envlen += strlen(value);
	envlen += 2;	// '=' + null byte 
	if(envlen > 255) 
		return(0);	// Are you crazy!? 

	// If NVRAM is found to be uninitialized, reinit it, 如果不可用 初始化. 
	if(nvram_invalid) 
		resetNorEnvSpace();
	// Find end of evironment strings 
	nvramsecbuf = (char *)malloc(NVRAM_SECSIZE);
	if(nvramsecbuf == 0) 
	{
		printf("Warning! Unable to malloc nvrambuffer!\n");
		return(-1);
	}
	memset(nvramsecbuf,0,NVRAM_SECSIZE);

	// Remove any current setting 
	unset_rom_env(name);//清除老数据
	
	nvram_get(nvramsecbuf);
	nvrambuf = nvramsecbuf + (NVRAM_OFFS & (NVRAM_SECSIZE - 1));

	{
		ep = nvrambuf+2;
		if(*ep != '\0') 
		{
			do 
			{
				while(*ep++ != '\0');
			} while(*ep++ != '\0');
			ep--;
		}
		if(((int)ep +NVRAM_SECSIZE  - (int)ep) < (envlen + 1)) //空间不够了NVRAM_SPECIAL_SIZE
		{
			free(nvramsecbuf);
			return(0);		// Bummer!
		}
		
		while(*name != '\0') 
		{
			*ep++ = *name++;
		}
		if(value != NULL) 
		{
			*ep++ = '=';
			while((*ep++ = *value++) != '\0');
		}
		else 
		{
			*ep++ = '\0';
		}
		*ep++ = '\0';	// End of env strings
	}
	cksum(nvrambuf,NVRAM_SECSIZE , 1);//NVRAM_SPECIAL_SIZE
	
	// Etheraddr is special case to save space 
	/*
	if (strcmp("ethaddr", name) == 0) 
	{
		char *s = value;
		int i;
		int32_t v;
		for(i = 0; i < 6; i++) 
		{
			gethex(&v, s, 2);
			hwethadr[i] = v;
			s += 3;         // Don't get to fancy here :-) 类似 00:12:34:56:78:0a
		} 
	} 
	else if(strcmp("pll_reg0", name) == 0)
		pll_reg0 = strtoul(value, 0, 0);
	else if(strcmp("pll_reg1", name) == 0)
		pll_reg1 = strtoul(value, 0, 0);
	else 
	{
		ep = nvrambuf+2;
		if(*ep != '\0') 
		{
			do 
			{
				while(*ep++ != '\0');
			} while(*ep++ != '\0');
			ep--;
		}
		if(((int)ep + NVRAM_SPECIAL_SIZE - (int)ep) < (envlen + 1)) 
		{
			free(nvramsecbuf);
			return(0);      // Bummer!
		}


		if(strcmp("heaptop", name) == 0) 
		{
			bcopy(nvrambuf+2, nvrambuf+2 + envlen,ep - nvrambuf+1);
			ep = nvrambuf+2;
			while(*name != '\0') 
			{
				*ep++ = *name++;
			}
			if(value != NULL) 
			{
				*ep++ = '=';
				while((*ep++ = *value++) != '\0');
			}
			else {
				*ep++ = '\0';
			}
		}
		else 
		{
			while(*name != '\0') 
			{
				*ep++ = *name++;
			}
			if(value != NULL) 
			{
				*ep++ = '=';
				while((*ep++ = *value++) != '\0');
			}
			else 
			{
				*ep++ = '\0';
			}
			*ep++ = '\0';   // End of env strings
		}
	}
	cksum(nvrambuf, NVRAM_SPECIAL_SIZE, 1);
	
	bcopy(hwethadr, &nvramsecbuf[ETHER_OFFS], 6);
	bcopy(&pll_reg0, &nvramsecbuf[PLL_OFFS], 4);
	bcopy(&pll_reg1, &nvramsecbuf[PLL_OFFS + 4], 4);
	*/
	nvram_put(nvramsecbuf);
	free(nvramsecbuf);
	return(1);
}

void map_rom_env(void)//从rom flash中获取配置信息
{
	char *ep;
	char env[NVRAM_SECSIZE];
	char *nvram;
	int i;
	
	nvram = (char *)malloc(NVRAM_SECSIZE);
	if(nvram==NULL)
	{
		printf("error info %s %s:%d\n", __FILE__ ,__func__, __LINE__);
		return;
	}
	memset(nvram,0,NVRAM_SECSIZE);
	
	nvram_get(nvram);//从rom中读取配置数据
	if(cksum(nvram,NVRAM_SECSIZE , 0) != 0) //检验数据失败 清除空间 NVRAM_SPECIAL_SIZE
	{
		nvram_invalid = 1;//复位后置可用标志
		resetNorEnvSpace();
		//网卡MAC需要指定下 不然默认为ff:ff:ff:ff:ff:ff
		//char hwethadrTemp[7]=default_mac;
		//bcopy(hwethadrTemp, hwethadr, 6); //dstling
		//memcpy(hwethadr,hwethadrTemp,6);
	}
	else 
	{
		nvram += NVRAM_OFFS;
		ep = nvram+2;//当前从flash获取的数据
		while(*ep != 0) 
		{
			char *val = 0, *p = env;
			i = 0;
			while((*p++ = *ep++) && (ep <= nvram +NVRAM_SECSIZE  - 1) && i++ < 255)//最多读255？ NVRAM_SPECIAL_SIZE
			{
				if((*(p - 1) == '=') && (val == NULL)) 
				{
					*(p - 1) = '\0';
					val = p;
				}
			}
			
			if(ep <= nvram +NVRAM_SECSIZE  - 1 && i < 255) //NVRAM_SPECIAL_SIZE
			{
				set_mem_env(env, val);
				
			}
			else 
			{
				nvram_invalid = 2;
				break;
			}
		}
		
		//bcopy(&nvram[ETHER_OFFS], hwethadr, 6);
	}


	/*
	 *  Ethernet address for Galileo ethernet is stored in the last
	 *  six bytes of nvram storage. Set environment to it.
	 */
	 
	/*	
	sprintf(env, "%02x:%02x:%02x:%02x:%02x:%02x", hwethadr[0], hwethadr[1],hwethadr[2], hwethadr[3], hwethadr[4], hwethadr[5]);
	printf("tgt_mapenv nor set MAC %02x:%02x:%02x:%02x:%02x:%02x\n", hwethadr[0], hwethadr[1],hwethadr[2], hwethadr[3], hwethadr[4], hwethadr[5]);
	(*func)("ethaddr", env);
	
	

#if defined(LS1BSOC) || defined(LS1CSOC)
	sprintf(env, "0x%08x", (pll_reg0=*(volatile int *)0xbfe78030));//从cpu里面读取的
	(*func)("pll_reg0", env);
#endif
	
	sprintf(env, "0x%08x", (pll_reg1=*(volatile int *)0xbfe78034));
	(*func)("pll_reg1", env);

	sprintf(env, "%d", memorysize / (1024 * 1024));
	(*func)("memsize", env);

	sprintf(env, "%d", memorysize_high / (1024 * 1024));
	(*func)("highmemsize", env);
	
	
	sprintf(env, "%d", md_pipefreq);
	(*func)("cpuclock", env);

	sprintf(env, "%d", md_cpufreq);
	(*func)("busclock", env);

	(*func)("systype", SYSTYPE);
	*/
	free(nvram);
}


int bootSpaceErase(int argc, char **argv)//清除rom空间 小心哦  清除后没得启动了
{
	printf("Boot Space Erasing...\n");
	spi_flash_erase_sector(0x00, bootRomSize-NVRAM_SECSIZE, 4096);
	printf("done!\n");
}

int clearEnv(int argc, char **argv)//清除配置空间
{
	printf("Env Space Erasing...\n");
	spi_flash_erase_sector(bootRomSize-NVRAM_SECSIZE, bootRomSize, 4096);
	printf("done!\n");
}
FINSH_FUNCTION_EXPORT(clearEnv,clearEnv sys argv);

char check_mac_format(char*macStr)//00:11:22:33:44:55
{
	if(strlen(macStr)!=17)
		return 0;
	if(macStr[2]!=':'||macStr[5]!=':'||macStr[8]!=':'||macStr[11]!=':'||macStr[14]!=':')
		return 0;
	
	int i=0;
	char macTmp[3];
	int hexValue;
	for(i=0;i+3<19;i+=3)
	{
		macTmp[0]=macStr[i];
		macTmp[1]=macStr[i+1];
		macTmp[2]='\0';
		
		hexValue=str_to_hex(macTmp);
		/*
		printf("macHex:%s,hexValue0x%08x\n",macHex,hexValue);
		*/
		if(hexValue>=0&&hexValue<0xff)
			continue;
		else//发生错误返回0
		{
			printf("check mac format error:%s\n",macStr);
			return 0;
		}
		
	}
	return 1;
}

char mac_str_to_hex(char*macStr,char*macHex)//将字符的mac转换成hex
{
	if(check_mac_format(macStr))
	{
		int i=0,j=0;
		char macTmp[3];
		int hexValue;
		for(i=0,j=0;i+3<19&&j<6;i+=3,j++)
		{
			macTmp[0]=macStr[i];
			macTmp[1]=macStr[i+1];
			macTmp[2]='\0';
			
			hexValue=str_to_hex(macTmp);
			/*
			printf("macHex:%s,hexValue0x%08x\n",macHex,hexValue);
			*/
			if(hexValue>=0&&hexValue<0xff)
			{
				macHex[j]=hexValue;
				continue;
			}
			else//发生错误返回0
			{
				return 0;
			}
		}
	}
	else return 0;
	return 1;
}

void do_setenv(char*name,char*value)
{
	if(striequ (name, ENV_MAC_NAME)&&!check_mac_format(value))//如果是设置mac 判断下格式对不对
	{
		return 0;
	}
		
	set_mem_env(name,value);
	set_rom_env(name,value);
}

int set(int argc, char **argv)
{
	struct envpair *ep;
	if(argc==1)
	{
		for (ep = envvar; ep < &envvar[NVAR]; ep++) 
		{
			if (ep->name) 
			{
				printf("%16s = %s\n",ep->name,ep->value);
				if(striequ(ep->name,PLL_REG0_NAME))
				{
					unsigned int pllreg0value=readl(LS1C_START_FREQ);
					printf("in cpu_reg\n%16s = 0x%08x\n",ep->name,pllreg0value);
				}
				if(striequ(ep->name,PLL_REG1_NAME))
				{
					unsigned int pllreg1value=readl(LS1C_CLK_DIV_PARAM);
					printf("in cpu_reg\n%16s = 0x%08x\n",ep->name,pllreg1value);
				}
			}
		}
	}
	else if(argv==2)
	{

	}
	else if(argc==3)
	{
		do_setenv(argv[1],argv[2]);
	}
}
FINSH_FUNCTION_EXPORT(set,set sys argv);

void envinit(void)
{
	int i;
    /* extract nvram variables into local copy */
    memset(envvar,0,sizeof(struct envpair)*NVAR);
    map_rom_env();
    envinited = 1;
	
	///*
    // set defaults (only if not set at all) 
    for (i = 0; stdenvtab[i].name; i++) 
	{
		if (!getenv (stdenvtab[i].name)) //没有找到环境变量 
		{
		  do_setenv(stdenvtab[i].name, stdenvtab[i].value);//设置一下
		}
    }
	//*/
	
	printf("Environment initialized.\n");
}







