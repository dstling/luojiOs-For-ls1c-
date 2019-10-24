#include <shell.h>

#define writel(val, addr) (*(volatile unsigned long *)(addr) = (val))
///*
#define LS1X_WDT_BASE		0xbfe5c060
#define LS1X_WDT_EN			(LS1X_WDT_BASE + 0x0)
#define LS1X_WDT_TIMER		(LS1X_WDT_BASE + 0x4)
#define LS1X_WDT_SET		(LS1X_WDT_BASE + 0x8)
//*/
void reboot(void)
{
	printf("rebooting...\n");
	writel(1, LS1X_WDT_EN);
	writel(50000, LS1X_WDT_TIMER);
	writel(1, LS1X_WDT_SET);
	while(1);
}

FINSH_FUNCTION_EXPORT(reboot,reboot board);


