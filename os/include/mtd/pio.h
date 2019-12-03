


#define __raw_readl(addr)   (*(volatile unsigned int *)(addr))
#define __raw_writel(l, addr)    ((*(volatile unsigned int *)(addr)) = (l))
#define writew(val, addr) (*(volatile unsigned short *)(addr) = (val))
#define readw(addr) (*(volatile unsigned short *)(addr))
#define writeb(val, addr) (*(volatile unsigned char *)(addr) = (val))
#define readb(addr) (*(volatile unsigned char *)(addr))
#define writel(val, addr) (*(volatile unsigned long *)(addr) = (val))
#define readl(addr) (*(volatile unsigned long *)(addr))









