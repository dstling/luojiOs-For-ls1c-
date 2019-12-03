#include <types.h>
#include <stdio.h>
#include <endian.h>
#include <string.h>
#include <filesys.h>

#include "elf.h"

#define DebugPR printf("loadelfDebug info %s %s:%d\n",__FILE__ , __func__, __LINE__)
#define perror printf

//#define __mips__
#define	NELF32ONLY	1

#define	NGZIP	1//默认为1 没有zlib的时候这个地方可设置0 这里调试时候临时改为0暂时减小移植工作量 gzip支持

#define SFLAG 0x0001		/* Don't clear symbols */
#define BFLAG 0x0002		/* Don't clear breakpoints */
#define EFLAG 0x0004		/* Don't clear exceptions */
#define AFLAG 0x0008		/* Don't add offset to symbols */
#define TFLAG 0x0010		/* Load at top of memory */
#define IFLAG 0x0020		/* Ignore checksum errors */
#define VFLAG 0x0040		/* verbose */
#define FFLAG 0x0080		/* Load into flash */
#define NFLAG 0x0100		/* Don't load symbols */
#define YFLAG 0x0200		/* Only load symbols */
#define WFLAG 0x0400		/* Swapped endianness */
#define UFLAG 0x0800		/* PMON upgrade flag */
#define KFLAG 0x1000		/* Load symbols for kernel debugger */ 
#define RFLAG 0x2000
#define OFLAG 0x4000
#define ZFLAG 0x8000
#define GFLAG 0x8001		/*load into nand flash */


#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))
#define CACHED_MEMORY_ADDR      0x80000000
#define PHYS_TO_CACHED(x)       ((unsigned)(x) | CACHED_MEMORY_ADDR)

int kmempos=0;//用于load的起始内存位置

unsigned long long dl_loffset=0;//没用吧
char *dl_Oloadbuffer=0;//没用吧


#if NGZIP > 0
//#include "../filesys/gzipfs.h"
#endif /* NGZIP */

static long	dl_entry;	/* entry address */
static long	dl_offset;	/* load offset */
static long	dl_minaddr;	/* minimum modified address */
static long	dl_maxaddr;	/* maximum modified address */
int dl_chksum=0;


extern long dl_minaddr;
extern long dl_maxaddr;
extern unsigned long long dl_loffset;
extern int highmemcpy(long long dst, long long src, long long count);
int memorysize;
static int	bootseg;
static unsigned long tablebase;
static unsigned int lastaddr=0;;
extern  char *dl_Oloadbuffer;
extern unsigned long long dl_loffset;
static int myflags;
#define CLIENTPC 0x80100000

typedef struct Sym {
    struct Sym     *next;
    unsigned long   value;
    char            name[1];
} Sym;
#define NHASH	127
static Sym 	*symhash[NHASH];

#define ALPHA	0
#define NUMERIC	1
static Sym	**symsort[2];

static int	maxsymlen;
static int	nsyms;

static void flushsort (int type)
{
    if (symsort[type]) 
	{
		free ((char *) symsort[type]);
		symsort[type] = 0;
    }
}
void clrhndlrs(void)
{
	
}

void clrsyms(void)
{
    Sym *sym, *nsym;
    int i;

    for (i = 0; i < NHASH; i++) 
	{
		for (sym = symhash[i]; sym; sym = nsym) 
		{
		    nsym = sym->next;
		    free ((char *) sym);
		}
		symhash[i] = 0;
    }

    flushsort (ALPHA);
    flushsort (NUMERIC);
    maxsymlen = 0;
    nsyms = 0;
}

static unsigned int strhash (const char *s)
{
    unsigned int h;
    for (h = 0; *s; s++) 
	{
        h = (h << 4) + *s;
        if (h > 0x0fffffff) 
		{
            h ^= (h >> 24) & 0xf0;
            h &= 0x0fffffff;
        }
    }
    return h;
}

static Sym *getsymbyname (char *name)
{
    unsigned h = strhash (name) % NHASH;
    Sym *sym;
    
    if (nsyms == 0)
      return 0;

    for (sym = symhash[h]; sym; sym = sym->next) 
	{
		if (name[0] == sym->name[0] && !strcmp(name+1, sym->name+1))
		  return sym;
    }
    return sym;
}

int newsym (char* name,u_int32_t value)
{
    Sym         *sym;
    unsigned int h;
    int		len;

    h = strhash (name) % NHASH;
    for (sym = symhash[h]; sym; sym = sym->next) 
	{
		if (sym->name[0] == name[0] && !strcmp(sym->name+1, name+1)) 
		{
		    if (sym->value != value) 
			{
				sym->value = value;
				flushsort (NUMERIC);
		    }
		    return (1);
		}
    }

    flushsort (ALPHA);
    flushsort (NUMERIC);

    len = strlen (name);
    sym = (Sym *) malloc (sizeof (Sym) + len);
    if (!sym)
      return (0);

    memcpy (sym->name, name, len + 1);
    sym->value = value;

    sym->next = symhash[h];
    symhash[h] = sym;

    if (len > maxsymlen)
      maxsymlen = len;
    nsyms++;
    return (1);
}

static void defsym (char*name,u_int32_t value)
{
    if(!getsymbyname (name))
		newsym (name, value);
}

void defsyms (u_int32_t minaddr,u_int32_t maxaddr,u_int32_t pc)
{
    extern int	    start[];
    defsym("Pmon", (unsigned long) start);
    if (minaddr != ~0)
		defsym("_ftext", minaddr);
    if (maxaddr != 0)
		defsym("etext", maxaddr);
    defsym ("start", pc);
}

void syminit (void)
{
	memorysize=kmempos;

	printf("syminit memorysize:0x%08x\n",memorysize);//syminit memorysize:0x01100000
    //defsyms((u_int32_t)sbrk(0) + 1024, memorysize | (CLIENTPC & 0xc0000000), CLIENTPC);
}

void dl_initialise (unsigned long offset, int flags)
{
	//printf("dl_initialise offset:0x%08x\n",offset);
    if (!(flags & SFLAG))
		clrsyms ();
    /*
    if (!(flags & BFLAG))
		clrbpts ();
	*/
    if (!(flags & EFLAG))
		clrhndlrs ();

    dl_minaddr = ~0;
    dl_maxaddr = 0;
    dl_chksum = 0;
    dl_offset = offset;
}

int highmemcpy(long long dst,long long src,long long count)
{
	memcpy(dst,src,count);
	return 0;
}

int highmemset(long long addr,char c,long long count)
{
	memset(addr,c,count);
	return 0;
}

int md_valid_load_addr(void * first_addr, void * last_addr)
{

	/*
	first_addr = CACHED_TO_PHYS(first_addr);
	last_addr = CACHED_TO_PHYS(last_addr);

	if(((first_addr < (paddr_t)CACHED_TO_PHYS(end)) && (last_addr >(paddr_t)CACHED_TO_PHYS(start))) ||
	   (last_addr > (paddr_t)kmempos)//memorysize
	   || address_in_heap(first_addr)//超过heaptop了
	   || address_in_heap(last_addr)//超过heaptop了
	   ) 
	{
		return(1);
	}
	*/   
	return(0);//应该到这
} 

int dl_checkloadaddr (void *vaddr, int size, int verbose)
{
	unsigned long addr = (unsigned long)vaddr;

	if (md_valid_load_addr(addr, addr + size)) 
	{
		if (verbose)
			printf ("\nattempt to load into mappable region");
		return 0;
	}
	return 1;
}

int dl_checksetloadaddr (void *vaddr, int size, int verbose)//检查load 设置的启动地址
{
	unsigned long addr = (unsigned long)vaddr;
	//printf("dl_checksetloadaddr dl_minaddr:0x%08x,dl_maxaddr:0x%08x,addr:0x%08x\n",dl_minaddr,dl_maxaddr,addr);
	if (!dl_checkloadaddr(vaddr, size, verbose))
		return (0);//不应该到这

	if (addr < dl_minaddr)
		dl_minaddr = addr;
	if (addr + size > dl_maxaddr)
		dl_maxaddr = addr + size;

	return 1;//应该到这里
}



void dl_setloadsyms (u_int32_t thread_pc)
{
	printf("dl_setloadsyms dl_minaddr:0x%08x,dl_maxaddr:0x%08x\n",dl_minaddr,dl_maxaddr);
	defsyms (dl_minaddr, dl_maxaddr, thread_pc);
}



//=======================================================================

static int bootread(int fd, void *addr, int size)
{
	int i;

	if (bootseg++ > 0)
		printf ("\b + ");
	//								0x80200000    638916
	printf ("0x%x/%d ", addr + dl_offset, size);

	if (!dl_checksetloadaddr (addr + dl_offset, size, 1))
	{
		DebugPR;
		return (-1);
	}

#if NGZIP > 0
	if (myflags&ZFLAG) 
	{
		if (myflags&OFLAG) 
		{
			if (lastaddr)
				dl_loffset += (unsigned long long)(addr-lastaddr);
			lastaddr = addr;
			i = 0;
			while (i<size) 
			{
				int i1;
				int len = min(0x1000,size-i);
				i1 = gz_read (fd,dl_Oloadbuffer,len);
				if (i1<0)
					break;
				highmemcpy(dl_loffset+i,dl_Oloadbuffer,i1);
				i += i1;
				if (i1<len)
					break;
			}
		}
		else 
			i = gz_read (fd, addr + dl_offset, size);
	} else
#endif /* NGZIP */

	{
		if(myflags&OFLAG)
		{
			//DebugPR;
			if(lastaddr)dl_loffset +=(unsigned long long)(addr-lastaddr);
			lastaddr=addr; 
			i=0;
			while(i<size)
			{
				int i1;
				int len=min(0x1000,size-i);
				i1=read (fd,dl_Oloadbuffer,len);
				if(i1<0)
					break;
				highmemcpy(dl_loffset+i,dl_Oloadbuffer,i1);
				i+=i1;
				if(i1<len)
					break;
			}
		}
		else 
		{
			//printf("\nbootread buf addr:0x%08X,size:%d\n",addr + dl_offset,size);
			i = read (fd, addr + dl_offset, size);//开始读取ph[0]指定位置的数据
		}
	}

	if (i < size) 
	{
		if (i >= 0)
			printf ( "\nread failed (corrupt object file?)");
		else
			printf ("\nsegment read");
		return (-1);
	}
	//printf("bootread size:%d\n",size);
	return size;
}

static int bootclear(int fd, void *addr, int size)
{

	if (bootseg++ > 0)
		printf ( "\b + ");
	printf ( "0x%x/%d(z) ", addr + dl_offset, size);

	if (!dl_checkloadaddr (addr + dl_offset, size, 1))
		return (-1);

	if (size > 0)
		if(myflags&OFLAG)
			highmemset((unsigned long long)(dl_loffset-lastaddr+(unsigned long)addr),0,size);
		else 
			memset (addr + dl_offset,0,size);
	    
	return size;
}

static Elf32_Shdr *elfgetshdr(int fd, Elf32_Ehdr *ep)//读取section header
{
	Elf32_Shdr *shtab;
	unsigned size = ep->e_shnum * sizeof(Elf32_Shdr);
	shtab = (Elf32_Shdr *) malloc (size);
	if (!shtab) {
		printf ("\nnot enough memory to read section headers");
		return (0);
	}

#if NGZIP > 0
if(myflags&ZFLAG){
	if (gz_lseek (fd, ep->e_shoff, SEEK_SET) != ep->e_shoff ||
	    gz_read (fd, shtab, size) != size) {
		perror ("\nsection headers");
		free (shtab);
		return (0);
	}
	}else
#endif /* NGZIP */
	if (lseek (fd, ep->e_shoff, SEEK_SET) != ep->e_shoff ||read (fd, shtab, size) != size) 
	{
		perror ("\nsection headers");
		free (shtab);
		return (0);
	}

	return (shtab);
}

static void *gettable(int size, char *name, int flags)
{
	unsigned long base;

	if( !(flags & KFLAG)) {
		/* temporarily use top of memory to hold a table */
		base = (tablebase - size) & ~7;
		if (base < dl_maxaddr) {
			printf ("\nnot enough memory for %s table", base);
			return 0;
		}
		tablebase = base;
	}
	else {
		/* Put table after loaded code to support kernel DDB */
		tablebase = roundup(tablebase, sizeof(long));
		base = tablebase;
		tablebase += size;
	}
	return (void *) base;
}

static void *readtable(int fd, int offs, void *base, int size, char *name, int flags)
{
#if NGZIP > 0
if(myflags&ZFLAG){
	if (gz_lseek (fd, offs, SEEK_SET) != offs ||
	    gz_read (fd, base, size) != size) {
		printf ( "\ncannot read %s table", name);
		return 0;
	}
	}else
#endif /* NGZIP */
	if (lseek (fd, offs, SEEK_SET) != offs ||
	    read (fd, base, size) != size) {
		printf ("\ncannot read %s table", name);
		return 0;
	}
	return (void *) base;
}

static int elfreadsyms(int fd, Elf32_Ehdr *eh, Elf32_Shdr *shtab, int flags)
{
	Elf32_Shdr *sh, *strh, *shstrh, *ksh;
	Elf32_Sym *symtab;
	Elf32_Ehdr *keh;
	char *shstrtab, *strtab, *symend;
	int nsym, offs, size, i;
	int *symptr;

	/* Fix up twirl */
	if (bootseg++ > 0) 
	{
		printf ("\b + ");
	}

	/*
	 *  If we are loading symbols to support kernel DDB symbol handling
	 *  make room for an ELF header at _end and after that a section
	 *  header. DDB then finds the symbols using the data put here.
	 */
	if(flags & KFLAG) 
	{
		tablebase = roundup(tablebase, sizeof(long));
		symptr = (int *)tablebase;
		tablebase += sizeof(int *) * 2;
		keh = (Elf32_Ehdr *)tablebase;
		tablebase += sizeof(Elf32_Ehdr); 
		tablebase = roundup(tablebase, sizeof(long));
		ksh = (Elf32_Shdr *)tablebase;
		tablebase += roundup((sizeof(Elf32_Shdr) * eh->e_shnum), sizeof(long)); 
		memcpy(ksh, shtab, roundup((sizeof(Elf32_Shdr) * eh->e_shnum), sizeof(long)));
		sh = ksh;
	}
	else 
	{
		sh = shtab;
	}
	shstrh = &sh[eh->e_shstrndx];

	for (i = 0; i < eh->e_shnum; sh++, i++) 
	{
		if (sh->sh_type == SHT_SYMTAB) 
		{
			break;
		}
	}
	if (i >= eh->e_shnum) 
	{
		return (0);
	}

	if(flags & KFLAG) 
	{
		strh = &ksh[sh->sh_link];
		nsym = sh->sh_size / sh->sh_entsize;
		offs = sh->sh_offset;
		size = sh->sh_size;
		printf ( "%d syms ", nsym);
	} 
	else 
	{
		strh = &shtab[sh->sh_link];
		nsym = (sh->sh_size / sh->sh_entsize) - sh->sh_info;
		offs = sh->sh_offset + (sh->sh_info * sh->sh_entsize);
		size = nsym * sh->sh_entsize;
		printf ( "%d syms ", nsym);
	}

	/*
	 *  Allocate tables in correct order so the kernel grooks it.
	 *  Then we read them in the order they are in the ELF file.
	 */
	shstrtab = gettable(shstrh->sh_size, "shstrtab", flags);
	strtab = gettable(strh->sh_size, "strtab", flags);
	symtab = gettable(size, "symtab", flags);
	symend = (char *)symtab + size;

	do {
		if(shstrh->sh_offset < offs && shstrh->sh_offset < strh->sh_offset) 
		{
			memset(shstrtab, 0, shstrh->sh_size);
			strcpy(shstrtab + shstrh->sh_name, ".shstrtab");
			strcpy(shstrtab + strh->sh_name, ".strtab");
			strcpy(shstrtab + sh->sh_name, ".symtab");
			shstrh->sh_offset = 0x7fffffff;
		}

		if (offs < strh->sh_offset && offs < shstrh->sh_offset) 
		{
			if (!(readtable(fd, offs, (void *)symtab, size, "sym", flags))) 
			{
				return (0);
			}
			offs = 0x7fffffff;
		}

		if (strh->sh_offset < offs && strh->sh_offset < shstrh->sh_offset) 
		{
			if (!(readtable (fd, strh->sh_offset, (void *)strtab,strh->sh_size, "string", flags))) 
			{
				return (0);
			}
			strh->sh_offset = 0x7fffffff;
		}
		if (offs == 0x7fffffff && strh->sh_offset == 0x7fffffff &&shstrh->sh_offset == 0x7fffffff) 
		{
			break;
		}
	} while(1);


	if(flags & KFLAG) 
	{
		/*
		 *  Update the kernel headers with the current info.
		 */
		shstrh->sh_offset = (Elf32_Off)shstrtab - (Elf32_Off)keh;
		strh->sh_offset = (Elf32_Off)strtab - (Elf32_Off)keh;
		sh->sh_offset = (Elf32_Off)symtab - (Elf32_Off)keh;
		memcpy(keh, eh, sizeof(Elf32_Ehdr));
		keh->e_phoff = 0;
		keh->e_shoff = sizeof(Elf32_Ehdr);
		keh->e_phentsize = 0;
		keh->e_phnum = 0;

		printf("\nKernel debugger symbols ELF hdr @ %p", keh);

		symptr[0] = (int)keh;
		symptr[1] = roundup((int)symend, sizeof(int));

	} 
	else 
	{
		/*
		 *  Add all global sybols to PMONs internal symbol table.
		 */
		for (i = 0; i < nsym; i++, symtab++) 
		{
			int type;
			dotik (4000, 0);
			if (symtab->st_shndx == SHN_UNDEF ||symtab->st_shndx == SHN_COMMON) 
			{
				continue;
			}

			type = ELF_ST_TYPE (symtab->st_info);
			if (type == STT_SECTION || type == STT_FILE)
			{
				continue;
			}

			/* only use globals and functions */
			if (ELF_ST_BIND(symtab->st_info) == STB_GLOBAL ||type == STT_FUNC)
			{
				if (symtab->st_name >= strh->sh_size) 
				{
					printf ( "\ncorrupt string pointer");
					return (0);
				}
			}
			if (!newsym (strtab + symtab->st_name, symtab->st_value)) 
			{
				printf ( "\nonly room for %d symbols", i);
				return (0);
			}
		}
	}
	return (1);
}

long load_elf (int fd, char *buf, int *n, int flags)
{
	Elf32_Ehdr *ep;
	Elf32_Phdr *phtab = 0;
	Elf32_Shdr *shtab = 0;
	unsigned int nbytes;
	int i;
	Elf32_Off highest_load = 0;
	bootseg = 0;
	myflags=flags;

#ifdef __mips__
	tablebase = PHYS_TO_CACHED(kmempos);//memorysize
	//printf("tablebase:0x%08x,kmempos:0x%08x\n",tablebase,kmempos);//tablebase:0x81100000,memorysize:0x01100000
#else
	tablebase =kmempos; //memorysize;
#endif

#if NGZIP > 0
	lseek(fd,*n,0);	
	read(fd,buf,2);
	if(((unsigned char)buf[0]==0x1f) && ((unsigned char)buf[1]==0x8b))
		flags |=ZFLAG;
	else 
		flags &=~ZFLAG;
	myflags=flags;
	lseek(fd,*n,0);	
	if(myflags&ZFLAG)
	{
		gz_open(fd);
		*n = 0;
		gz_lseek (fd, 0, SEEK_SET);
	}
#endif /* NGZIP */

	//printf("load_elf buf addr:0x%08X\n",buf);
	ep = (Elf32_Ehdr *)buf;
	//printf("load_elf ep addr:0x%08X sizeof(*ep):%d *n=%d\n",ep ,sizeof(*ep),*n);
	if (sizeof(*ep) > *n) 
	{
#if NGZIP > 0
		if(myflags&ZFLAG)
			*n += gz_read (fd, buf+*n, sizeof(*ep)-*n);
		else 
#endif /* NGZIP */
		{
			lseek(fd,*n,0);	
			*n += read(fd, buf+*n, sizeof(*ep)-*n);//先从tftp服务器读取elf header sizeof(*ep)-*n大小的数据到buf
			printf("read elf head size:%d\n",*n);
			dataHexPrint2(ep, 52,0,"read elf head2");
		}
		if (*n < sizeof(*ep)) 
		{
#if NGZIP > 0
		if(myflags&ZFLAG)			
			gz_close(fd);
#endif /* NGZIP */
			return -1;
		}
	}

	/* check header validity */
	if (ep->e_ident[EI_MAG0] != ELFMAG0 ||ep->e_ident[EI_MAG1] != ELFMAG1 ||ep->e_ident[EI_MAG2] != ELFMAG2 ||ep->e_ident[EI_MAG3] != ELFMAG3) 
	{
#if NGZIP > 0
		if(myflags&ZFLAG)		
			gz_close(fd);
#endif /* NGZIP */
		printf("load elf readed header error.\n");
		return (-1);
	}

	printf ( "(elf)\n");
	{
		char *nogood = (char *)0;
#if (NELF32ONLY==0)
		if (ep->e_ident[EI_CLASS] == ELFCLASS64)//判断是否是64位体系
			return load_elf64(fd, buf, n, flags);
#endif

		if (ep->e_ident[EI_CLASS] != ELFCLASS32) //01 32bit
			nogood = "not 32-bit";
		else if (
#if BYTE_ORDER == BIG_ENDIAN
			 ep->e_ident[EI_DATA] != ELFDATA2MSB
#endif
#if BYTE_ORDER == LITTLE_ENDIAN  //小端体系
			 ep->e_ident[EI_DATA] != ELFDATA2LSB //01 LSB
#endif
			  )
			nogood = "incorrect endianess";
		else if (ep->e_ident[EI_VERSION] != EV_CURRENT) //01
			nogood = "version not current";
		else if (
#ifdef powerpc
			 ep->e_machine != EM_PPC
#else /* default is MIPS */
#define GREENHILLS_HACK
#ifdef GREENHILLS_HACK
			 ep->e_machine != 10 && 
#endif
			 ep->e_machine != EM_MIPS //head 19位=8 mips体系
#endif
			  )
			nogood = "incorrect machine type";

		if (nogood) 
		{
			printf ("Invalid ELF: %s\n", nogood);
#if NGZIP > 0
			if(myflags&ZFLAG)			
				gz_close(fd);
#endif /* NGZIP */
			return -2;
		}
	}//ELF Header 判断结束

	/* Is there a program header? *///判断程序header是否正确 
	if (ep->e_phoff == 0 || ep->e_phnum == 0 ||ep->e_phentsize != sizeof(Elf32_Phdr)) 
	{
		printf ( "missing program header (not executable)\n");
#if NGZIP > 0
	if(myflags&ZFLAG)		
		gz_close(fd);
#endif /* NGZIP */
		return (-2);
	}

	//printf("ep->e_phoff:0x%08X ep->e_phnum:%d ep->e_phentsize:%d sizeof(Elf32_Phdr):%d\n",ep->e_phoff,ep->e_phnum,ep->e_phentsize,sizeof(Elf32_Phdr));
	//ep->e_phoff:0x00000034 ep->e_phnum:2 ep->e_phentsize:32 sizeof(Elf32_Phdr):32
	//printf("ep->e_shoff:0x%08X ep->e_shnum:%d ep->e_shentsize:%d sizeof(Elf32_Shdr):%d\n",ep->e_shoff,ep->e_shnum,ep->e_shentsize,sizeof(Elf32_Shdr));
	//ep->e_shoff:0x001C7F54 ep->e_shnum:25 ep->e_shentsize:40 sizeof(Elf32_Shdr):40
	/* Load program header */
#if _ORIG_CODE_
	nbytes = ep->e_phnum * sizeof(Elf32_Phdr);
#else
	/* XXX: We need to figure out why it works by adding 32!!!! 为什么要+32*/
	nbytes = ep->e_phnum * sizeof(Elf32_Phdr)+32;//size 96=32*3
#endif
	phtab = (Elf32_Phdr *) malloc (nbytes);
	if (!phtab) 
	{
		printf ("\nnot enough memory to read program headers");
#if NGZIP > 0
		if(myflags&ZFLAG)		
			gz_close(fd);
#endif /* NGZIP */
		return (-2);
	}

#if NGZIP > 0
	if(myflags&ZFLAG)
	{
		if (gz_lseek (fd, ep->e_phoff, SEEK_SET) != ep->e_phoff ||gz_read (fd, (void *)phtab, nbytes) != nbytes) 
		{
			perror ("program header");
			free (phtab);
			gz_close(fd);
			return (-2);
		}
	}
	else
#endif /* NGZIP */
	//读取program header ep->e_entry在这里就已经被指定了
///*
	{
	//printf("load_elf dstling01\n");
		off_t ret=lseek(fd, (off_t)ep->e_phoff, SEEK_SET);
		if(ret==(off_t)ep->e_phoff)
		{
			unsigned int readRet=read(fd,(void *)phtab,nbytes);
			if(readRet!= nbytes)
			{
				perror ("program header readRet!=nbytes,readRet:%d,nbytes:%d,ret:%d",readRet,nbytes,(int)ret);
				free (phtab);
				return (-2);
			}
			//printf("2program readRet:%d,nbytes:%d,ret:%d\n",readRet,nbytes,(int)ret);
		}
		else
		{
			perror("lseek error\n");
			printf("ep->e_phoff:%d,ret:%lld,SEEK_SET:%d\n",ep->e_phoff,ret,SEEK_SET);
			free (phtab);
			return (-2);
		}
	}
	//printf("load_elf dstling02\n");
//*/

/*
	printf("load_elf dstling01\n");

	if (lseek (fd, ep->e_phoff, SEEK_SET) != ep->e_phoff||read (fd,(void *)phtab,nbytes)!= nbytes) //这里read可能不在需要去硬件里面读取了
	{
		perror ("program header");
		free (phtab);
		return (-2);
	}
		
	printf("load_elf dstling02\n");
*/
	//printf("ep->e_shoff:0x%08X phtab[0].p_offset:0x%08X,ep->e_entry:0x%08X.\n",ep->e_shoff ,phtab[0].p_offset,ep->e_entry);
	//show_hex(phtab,nbytes,ep->e_phoff);
	
	/*
	 * From now on we've got no guarantee about the file order, 
	 * even where the section header is.  Hopefully most linkers
	 * will put the section header after the program header, when
	 * they know that the executable is not demand paged.  We assume
	 * that the symbol and string tables always follow the program 
	 * segments.
	 *从现在起，我们无法保证文件顺序，甚至在节头所在的地方也是如此。
	 *希望大多数链接器在知道可执行文件没有被请求分页时，将节标题放在
	 *程序标题之后。我们假设符号和字符串表总是跟随程序段。
	 */

	/* read section table (if before first program segment) 读取节表（如果在第一个程序段之前）*/
	//						0x001C7F54    
	if (!(flags & NFLAG) && ep->e_shoff < phtab[0].p_offset)//section header 在program header前面才读取
		shtab = elfgetshdr (fd, ep);//读取section header
	//printf("shtab addr:0x%08X\n",shtab);
	
	/* load program segments */
	if (!(flags & YFLAG)) 
	{
		/* We cope with a badly sorted program header, as produced by 
		 * older versions of the GNU linker, by loading the segments
		 * in file offset order, not in program header order. */
		while (1) 
		{
			Elf32_Off lowest_offset = ~0;
			Elf32_Phdr *ph = 0;

			/* find nearest loadable segment */
			for (i = 0; i < ep->e_phnum; i++)
				if (phtab[i].p_type == PT_LOAD && phtab[i].p_offset < lowest_offset) 
				{
					ph = &phtab[i];
					lowest_offset = ph->p_offset;
					//printf("ph p_vaddr:0x%08X p_filesz:%d p_offset:0x%08X ph->p_memsz:%d \n",ph->p_vaddr,ph->p_filesz,ph->p_offset,ph->p_memsz);
				}
				
			if (!ph)
				break;		/* none found, finished */
			//printf("ph p_vaddr:0x%08X p_filesz:%d p_offset:0x%08X ph->p_memsz:%d \n",ph->p_vaddr,ph->p_filesz,ph->p_offset,ph->p_memsz);
			/* load the segment */
			if (ph->p_filesz) 
			{
#if NGZIP > 0
				if(myflags&ZFLAG)
				{
					if (gz_lseek (fd, ph->p_offset, SEEK_SET) != ph->p_offset) 
					{
						printf ( "seek failed (corrupt object file?)\n");
						if (shtab)
							free (shtab);
						free (phtab);
						gz_close(fd);
						return (-2);
					}
				}
				else
#endif /* NGZIP */
				if (lseek(fd,ph->p_offset,SEEK_SET)!= ph->p_offset) 
				{
					printf ( "seek failed (corrupt object file?)\n");
					if (shtab)
						free (shtab);
					free (phtab);
					return (-2);
				}
				//读取ph->p_offset指向的文件的数据至ph->p_vaddr内存				p_vaddr:0x80200000	638916			
				if (bootread(fd,(void*)ph->p_vaddr,ph->p_filesz)!= ph->p_filesz) //到这里有问题了
				{
					if (shtab) 
						free (shtab);
					free (phtab);
#if NGZIP > 0
					if(myflags&ZFLAG)					
						gz_close(fd);
#endif /* NGZIP */
					return (-2);
				}
			}
			
			if((ph->p_vaddr + ph->p_memsz)> highest_load) 
				highest_load = ph->p_vaddr + ph->p_memsz;

			if (ph->p_filesz < ph->p_memsz)//文件中占用的size 小于 内存映像中占用的size
				bootclear (fd, (void *)ph->p_vaddr + ph->p_filesz, ph->p_memsz - ph->p_filesz);//将多出来的内存ph->p_vaddr + ph->p_filesz后面的地址清零
			ph->p_type = PT_NULL; /* remove from consideration */
		}

	}

	if (flags & KFLAG) 
	{
		highest_load = roundup(highest_load, sizeof(long));
		tablebase = highest_load;
	}
	if (!(flags & NFLAG)) 
	{
		/* read section table (if after last program segment) */
		if (!shtab)
			shtab = elfgetshdr (fd, ep);
		if (shtab) 
		{
			elfreadsyms (fd, ep, shtab, flags);//读取syms
			free (shtab);
		}
	}

	free (phtab);
#if NGZIP > 0
	if(myflags&ZFLAG)	
		gz_close(fd);
#endif /* NGZIP */

	int retaddr=ep->e_entry + dl_offset;
	//int retadj=retaddr&0x0000ffff|0x80300000;
	return (retaddr);
	//return (ep->e_entry + dl_offset);
}


