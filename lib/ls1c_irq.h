

#ifndef __IRQ_H
#define __IRQ_H


// 中断号
#define LS1C_ACPI_IRQ	0
#define LS1C_HPET_IRQ	1
//#define LS1C_UART00_IRQ	0  // linux中是0，v1.4版本的1c手册中是2，暂屏蔽，待确认
//#define LS1C_UART01_IRQ 1   // 全功能串口0可以分为两个四线串口UART00和UART01
#define LS1C_UART1_IRQ	4
#define LS1C_UART2_IRQ	5
#define LS1C_CAN0_IRQ	6
#define LS1C_CAN1_IRQ	7
#define LS1C_SPI0_IRQ	8
#define LS1C_SPI1_IRQ	9
#define LS1C_AC97_IRQ	10
#define LS1C_MS_IRQ		11
#define LS1C_KB_IRQ		12
#define LS1C_DMA0_IRQ	13
#define LS1C_DMA1_IRQ	14
#define LS1C_DMA2_IRQ   15
#define LS1C_NAND_IRQ	16
#define LS1C_PWM0_IRQ	17
#define LS1C_PWM1_IRQ	18
#define LS1C_PWM2_IRQ	19
#define LS1C_PWM3_IRQ	20
#define LS1C_RTC_INT0_IRQ  21
#define LS1C_RTC_INT1_IRQ  22
#define LS1C_RTC_INT2_IRQ  23
#define LS1C_UART3_IRQ  29
#define LS1C_ADC_IRQ    30
#define LS1C_SDIO_IRQ   31


#define LS1C_EHCI_IRQ	(32+0)
#define LS1C_OHCI_IRQ	(32+1)
#define LS1C_OTG_IRQ    (32+2)
#define LS1C_MAC_IRQ    (32+3)
#define LS1C_CAM_IRQ    (32+4)
#define LS1C_UART4_IRQ  (32+5)
#define LS1C_UART5_IRQ  (32+6)
#define LS1C_UART6_IRQ  (32+7)
#define LS1C_UART7_IRQ  (32+8)
#define LS1C_UART8_IRQ  (32+9)
#define LS1C_UART9_IRQ  (32+13)
#define LS1C_UART10_IRQ (32+14)
#define LS1C_UART11_IRQ (32+15)
#define LS1C_I2C2_IRQ   (32+17)
#define LS1C_I2C1_IRQ   (32+18)
#define LS1C_I2C0_IRQ   (32+19)


#define LS1C_GPIO_IRQ 64
#define LS1C_GPIO_FIRST_IRQ 64
#define LS1C_GPIO_IRQ_COUNT 96
#define LS1C_GPIO_LAST_IRQ  (LS1C_GPIO_FIRST_IRQ + LS1C_GPIO_IRQ_COUNT-1)


#define LS1C_LAST_IRQ 159

// 龙芯1c的中断分为五组，每组32个
#define LS1C_NR_IRQS    (32*5)


// GPIO编号和中断号之间的互相转换
#define LS1C_GPIO_TO_IRQ(GPIOn)     (LS1C_GPIO_FIRST_IRQ + (GPIOn))
#define LS1C_IRQ_TO_GPIO(IRQn)      ((IRQn) - LS1C_GPIO_FIRST_IRQ)


// 定义中断处理函数
typedef void (*irq_handler_t)(int IRQn, void *param);
typedef struct irq_desc
{
    irq_handler_t   handler;
    void            *param;
}irq_desc_t;


// 初始化异常
void exception_init(void);


/*
 * 使能指定中断
 * @IRQn 中断号
 */
void irq_enable(int IRQn);


/*
 * 禁止指定中断
 * @IRQn 中断号
 */
void irq_disable(int IRQn);


/*
 * 设置中断处理函数
 * @IRQn 中断号
 * @new_handler 新设置的中断处理函数
 * @param 传递给中断处理函数的参数
 * @ret 之前的中断处理函数
 */
irq_handler_t irq_install(int IRQn, irq_handler_t new_handler, void *param);


struct ls1c_intc_regs
{
	volatile unsigned int int_isr;
	volatile unsigned int int_en;
	volatile unsigned int int_set;
	volatile unsigned int int_clr;		/* offset 0x10*/
	volatile unsigned int int_pol;
   	volatile unsigned int int_edge;		/* offset 0 */
}; 


#endif

