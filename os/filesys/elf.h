
typedef unsigned short	Elf32_Half;
typedef unsigned int	Elf32_Word;
typedef signed int	Elf32_Sword;
typedef unsigned int	Elf32_Off;
typedef unsigned int	Elf32_Addr;
typedef unsigned char	Elf_Char;

/*
 * File Header 
 */

#define EI_NIDENT	16 

typedef struct {
    Elf_Char	e_ident[EI_NIDENT];	//16*1 ELF Identification  标识该文件为对象文件，并且提供了机器独立的数据，其用来解释文件内容
    Elf32_Half	e_type;				//2 对象文件的类型
    Elf32_Half	e_machine;			//2 适用的计算机架构 EM_MIPS          8  MIPS 
    Elf32_Word	e_version;			//4 对象文件版本
    Elf32_Addr	e_entry;			//4 系统接管该进程控制的第一条指令的逻辑地址
    Elf32_Off	e_phoff;			//4 包含了program header table的文件偏移地址
    Elf32_Off	e_shoff;			//4 section header table的文件偏移地址
    Elf32_Word	e_flags;			//4 文件与处理器相关的标识
    Elf32_Half	e_ehsize;			//2 ELF header的尺寸
    Elf32_Half	e_phentsize;		//2 program header table中一条表项的字节数
    Elf32_Half	e_phnum;			//2 program header table中表项的数目
    Elf32_Half	e_shentsize;		//2 section header table中一条表项的字节数
    Elf32_Half	e_shnum;			//2 section header table中表项的数目
    Elf32_Half	e_shstrndx;			//2 section name string table（段名称字符串表） 独立构成一个section 该section在section header table中表项的索引号
} Elf32_Ehdr;

/* e_indent */
#define EI_MAG0		0		/* File identification byte 0 index */ //文件的头4个字节包含了特征码，其用来表明该文件是一个ELF对象文件
#define EI_MAG1		1		/* File identification byte 1 index */
#define EI_MAG2		2		/* File identification byte 2 index */
#define EI_MAG3		3		/* File identification byte 3 index */

#define EI_CLASS	4		/* File class */ //不同总线宽度的机器中通用
#define  ELFCLASSNONE	 0		 /* Invalid class */
#define  ELFCLASS32	 1		 /* 32-bit objects */ //32支持
#define  ELFCLASS64	 2		 /* 64-bit objects *///64位架构

#define EI_DATA		5		/* Data encoding *///规定了对象文件中和处理器相关的数据的如何编码 
#define  ELFDATANONE	 0		 /* Invalid data encoding */
#define  ELFDATA2LSB	 1		 /* 2's complement, little endian */
#define  ELFDATA2MSB	 2		 /* 2's complement, big endian */

#define EI_VERSION	6		/* File version *///指定了EFL header版本号
#define EI_PAD		7		/* Start of padding bytes *///该值标明了在e_ident中无用字节的开始，这些字节被设置为0

#define ELFMAG0		0x7F		/* Magic number byte 0 */
#define ELFMAG1		'E'		/* Magic number byte 1 */
#define ELFMAG2		'L'		/* Magic number byte 2 */
#define ELFMAG3		'F'		/* Magic number byte 3 */

/* e_type */
#define ET_NONE		0		/* No file type */
#define ET_REL		1		/* Relocatable file */
#define ET_EXEC		2		/* Executable file */
#define ET_DYN		3		/* Shared object file */
#define ET_CORE		4		/* Core file */
#define ET_LOPROC	0xFF00		/* Processor-specific */
#define ET_HIPROC	0xFFFF		/* Processor-specific */

/* e_machine */
#define EM_NONE		0		/* No machine */
#define EM_M32		1		/* AT&T WE 32100 */
#define EM_SPARC	2		/* SUN SPARC */
#define EM_386		3		/* Intel 80386 */
#define EM_68K		4		/* Motorola m68k family */
#define EM_88K		5		/* Motorola m88k family */
#define EM_860		7		/* Intel 80860 */
#define EM_MIPS		8		/* MIPS R3000  */

#define EM_PPC          20              /* PowerPC */

/* e_version */
#define EV_NONE		0		/* Invalid ELF version */
#define EV_CURRENT	1		/* Current version */


/*
 * Program Header 可执行或共享对象文件 每一个表项都描述了一个段或者其他系统用来为程序运行做准备的信息
 *只对可执行和共享对象文件有意义
 */
typedef struct {
  Elf32_Word	p_type;			/* Identifies program segment type *///该数组元素所描述的段的类型以及如何解释该数组元素的属性
  Elf32_Off		p_offset;		/* Segment file offset *///该成员段的第一字节至文件开始处的偏移
  Elf32_Addr	p_vaddr;		/* Segment virtual address *///该成员给出了段的第一字节在内存中的逻辑地址
  Elf32_Addr	p_paddr;		/* Segment physical address *///基本无用
  Elf32_Word	p_filesz;		/* Segment size in file *///该成员给出了该段在文件中占用的字节数
  Elf32_Word	p_memsz;		/* Segment size in memory *///该成员给出了该段在内存映像中占用的字节
  Elf32_Word	p_flags;		/* Segment flags *///该成员给出了段相关的标志
  Elf32_Word	p_align;		/* Segment alignment, file & memory *///当程序加载后，可加载的进程段的p_vaddr和p_offset取模page size(4KB)之后的值必须相等
} Elf32_Phdr;


/* p_type */
#define	PT_NULL		0		/* Program header table entry unused */
#define PT_LOAD		1		/* Loadable program segment */
#define PT_DYNAMIC	2		/* Dynamic linking information */
#define PT_INTERP	3		/* Program interpreter */
#define PT_NOTE		4		/* Auxiliary information */
#define PT_SHLIB	5		/* Reserved, unspecified semantics */
#define PT_PHDR		6		/* Entry for header table itself */
#define PT_LOPROC	0x70000000	/* Processor-specific */
#define PT_HIPROC	0x7FFFFFFF	/* Processor-specific */

/* p_flags */
#define PF_X		(1 << 0)	/* Segment is executable */
#define PF_W		(1 << 1)	/* Segment is writable */
#define PF_R		(1 << 2)	/* Segment is readable */
#define PF_MASKPROC	0xF0000000	/* Processor-specific reserved bits */


/*
 * Section Header  section header tables
 *	ELF header中的e_shoff成员给出了section header table到文件头的偏移（按字节）
 */
typedef struct {
    Elf32_Word	sh_name;//该成员指定了section的名称
    Elf32_Word	sh_type;//section分类
    Elf32_Word	sh_flags;//使用1bit标志来描述大量的属性
    Elf32_Addr	sh_addr;//如果某个section将会出现在进程的内存映射中，该成员给出的就是该section第一个字节驻留在内存中的地址
    Elf32_Off	sh_offset;//该成员给出了从文件开始处到section中第一个字节的偏移（按字节）
    Elf32_Word	sh_size;//该成员给出了section的大小尺寸（按字节）
    Elf32_Word	sh_link;//一个section header table的索引链接
    Elf32_Word	sh_info;//额外的信息
    Elf32_Word	sh_addralign;//section可能会有地址对齐约束
    Elf32_Word	sh_entsize;//包含的是一组固定大小表项的表
} Elf32_Shdr;

/* sh_type */
#define SHT_NULL	0		/* Section header table entry unused *////该值表示该section header是不活动的，其没有对应的section，此时section header中其他的成员都没有意义
#define SHT_PROGBITS	1		/* Program specific (private) data *///包含了程序自定义信息，其格式和含义由程序自行决定
#define SHT_SYMTAB	2		/* Link editing symbol table *///连接器提供了符号
#define SHT_STRTAB	3		/* A string table */
#define SHT_RELA	4		/* Relocation entries with addends *///重组表项
#define SHT_HASH	5		/* A symbol hash table *///符号hash表
#define SHT_DYNAMIC	6		/* Information for dynamic linking */
#define SHT_NOTE	7		/* Information that marks file */
#define SHT_NOBITS	8		/* Section occupies no space in file */
#define SHT_REL		9		/* Relocation entries, no addends */
#define SHT_SHLIB	10		/* Reserved, unspecified semantics */
#define SHT_DYNSYM	11		/* Dynamic linking symbol table */
#define SHT_LOPROC	0x70000000	/* Processor-specific semantics, lo */
#define SHT_HIPROC	0x7FFFFFFF	/* Processor-specific semantics, hi */
#define SHT_LOUSER	0x80000000	/* Application-specific semantics */
#define SHT_HIUSER	0x8FFFFFFF	/* Application-specific semantics */

/* sh_flags */
#define SHF_WRITE	(1 << 0)	/* Writable data during execution */
#define SHF_ALLOC	(1 << 1)	/* Occupies memory during execution */
#define SHF_EXECINSTR	(1 << 2)	/* Executable machine instructions */
#define SHF_MASKPROC	0xF0000000	/* Processor-specific semantics */


/*
 * Symbol Table
 */
typedef struct {
    Elf32_Word	st_name;
    Elf32_Addr	st_value;
    Elf32_Word	st_size;
    Elf_Char	st_info;
    Elf_Char	st_other;
    Elf32_Half	st_shndx;
} Elf32_Sym;


#define ELF_ST_BIND(val)		(((unsigned int)(val)) >> 4)
#define ELF_ST_TYPE(val)		((val) & 0xF)
#define ELF_ST_INFO(bind,type)		(((bind) << 4) | ((type) & 0xF))

/* symbol binding */
#define STB_LOCAL	0		/* Symbol not visible outside obj */
#define STB_GLOBAL	1		/* Symbol visible outside obj */
#define STB_WEAK	2		/* Like globals, lower precedence */
#define STB_LOPROC	13		/* Application-specific semantics */
#define STB_HIPROC	15		/* Application-specific semantics */

/* symbol type */
#define STT_NOTYPE	0		/* Symbol type is unspecified */
#define STT_OBJECT	1		/* Symbol is a data object */
#define STT_FUNC	2		/* Symbol is a code object */
#define STT_SECTION	3		/* Symbol associated with a section */
#define STT_FILE	4		/* Symbol gives a file name */
#define STT_LOPROC	13		/* Application-specific semantics */
#define STT_HIPROC	15		/* Application-specific semantics */

/* special values st_shndx */
#define SHN_UNDEF	0		/* Undefined section reference */
#define SHN_LORESERV	0xFF00		/* Begin range of reserved indices */
#define SHN_LOPROC	0xFF00		/* Begin range of appl-specific */
#define SHN_HIPROC	0xFF1F		/* End range of appl-specific */
#define SHN_ABS		0xFFF1		/* Associated symbol is absolute */
#define SHN_COMMON	0xFFF2		/* Associated symbol is in common */
#define SHN_HIRESERVE	0xFFFF		/* End range of reserved indices */
/* for elf64 */

typedef unsigned long long  __u64;
typedef unsigned int __u32;
typedef unsigned short __u16;
typedef signed int __s32;
typedef signed short __s16;
typedef long long __s64;

/* 64-bit ELF base types. */
typedef __u64	Elf64_Addr;
typedef __u16	Elf64_Half;
typedef __s16	Elf64_SHalf;
typedef __u64	Elf64_Off;
typedef __s32	Elf64_Sword;
typedef __u32	Elf64_Word;
typedef __u64	Elf64_Xword;
typedef __s64	Elf64_Sxword;

typedef struct {
  Elf64_Sxword d_tag;		/* entry tag value */
  union {
    Elf64_Xword d_val;
    Elf64_Addr d_ptr;
  } d_un;
} Elf64_Dyn;


#define ELF64_R_SYM(i)			((i) >> 32)
#define ELF64_R_TYPE(i)			((i) & 0xffffffff)


typedef struct elf64_rel {
  Elf64_Addr r_offset;	/* Location at which to apply the action */
  Elf64_Xword r_info;	/* index and type of relocation */
} Elf64_Rel;

typedef struct elf64_rela {
  Elf64_Addr r_offset;	/* Location at which to apply the action */
  Elf64_Xword r_info;	/* index and type of relocation */
  Elf64_Sxword r_addend;	/* Constant addend used to compute value */
} Elf64_Rela;

typedef struct elf64_sym {
  Elf64_Word st_name;		/* Symbol name, index in string tbl */
  unsigned char	st_info;	/* Type and binding attributes */
  unsigned char	st_other;	/* No defined meaning, 0 */
  Elf64_Half st_shndx;		/* Associated section index */
  Elf64_Addr st_value;		/* Value of the symbol */
  Elf64_Xword st_size;		/* Associated symbol size */
} Elf64_Sym;


typedef struct elf64_hdr {
  unsigned char	e_ident[16];		/* ELF "magic number" */
  Elf64_Half e_type;
  Elf64_Half e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;		/* Entry point virtual address */
  Elf64_Off e_phoff;		/* Program header table file offset */
  Elf64_Off e_shoff;		/* Section header table file offset */
  Elf64_Word e_flags;
  Elf64_Half e_ehsize;
  Elf64_Half e_phentsize;
  Elf64_Half e_phnum;
  Elf64_Half e_shentsize;
  Elf64_Half e_shnum;
  Elf64_Half e_shstrndx;
} Elf64_Ehdr;


typedef struct elf64_phdr {
  Elf64_Word p_type;
  Elf64_Word p_flags;
  Elf64_Off p_offset;		/* Segment file offset */
  Elf64_Addr p_vaddr;		/* Segment virtual address */
  Elf64_Addr p_paddr;		/* Segment physical address */
  Elf64_Xword p_filesz;		/* Segment size in file */
  Elf64_Xword p_memsz;		/* Segment size in memory */
  Elf64_Xword p_align;		/* Segment alignment, file & memory */
} Elf64_Phdr;

typedef struct elf64_shdr {
  Elf64_Word sh_name;		/* Section name, index in string tbl */
  Elf64_Word sh_type;		/* Type of section */
  Elf64_Xword sh_flags;		/* Miscellaneous section attributes */
  Elf64_Addr sh_addr;		/* Section virtual addr at execution */
  Elf64_Off sh_offset;		/* Section file offset */
  Elf64_Xword sh_size;		/* Size of section in bytes */
  Elf64_Word sh_link;		/* Index of another section */
  Elf64_Word sh_info;		/* Additional section information */
  Elf64_Xword sh_addralign;	/* Section alignment */
  Elf64_Xword sh_entsize;	/* Entry size if section holds table */
} Elf64_Shdr;

/* Note header in a PT_NOTE section */
typedef struct elf64_note {
  Elf64_Word n_namesz;	/* Name size */
  Elf64_Word n_descsz;	/* Content size */
  Elf64_Word n_type;	/* Content type */
} Elf64_Nhdr;


#if ELF_CLASS == ELFCLASS32

extern Elf32_Dyn _DYNAMIC [];
#define elfhdr		elf32_hdr
#define elf_phdr	elf32_phdr
#define elf_note	elf32_note

#else

extern Elf64_Dyn _DYNAMIC [];
#define elfhdr		elf64_hdr
#define elf_phdr	elf64_phdr
#define elf_note	elf64_note

#endif
