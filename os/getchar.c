

#include <ls1c_irq.h>
#include <ls1c_uart.h>
#include "rtdef.h"


#define	IIR_RXTOUT	0xc	/* receive timeout */
#define	IIR_RXRDY	0x4	/* receive ready */
#define LS1C_UART_IIR_OFFSET            (2)
#define MAX_INTR            (LS1C_NR_IRQS)

#define UART_BUF_SIZE 16

struct rt_uart_ls1c
{
    ls1c_uart_t UARTx;
    unsigned int IRQ;
};

struct rt_uart_ls1c uart2 =
{
    LS1C_UART2,
    LS1C_UART2_IRQ,
};

struct rt_serial_rx_fifo
{
	unsigned char *buffer;
	unsigned short put_index, get_index;
	int is_full;
};

struct rt_serial_rx_fifo* uart2_rx_fifo;

extern irq_desc_t irq_handlers[];
extern struct ls1c_intc_regs volatile *ls1c_hw0_icregs ;


static int ls1c_uart_getc(void)
{
    struct rt_uart_ls1c *uart_dev = &uart2;
    void *uart_base = uart_get_base(uart_dev->UARTx);
    if ( 1& reg_read_8(uart_base + 5))//LSR_RXRDY LS1C_UART_LSR_OFFSET
    {
        return reg_read_8(uart_base + 0);//LS1C_UART_DAT_OFFSET
    }
    return -1;
}

void rt_hw_serial_isr(void)
{
    int ch = -1;
    long level;
    struct rt_serial_rx_fifo* rx_fifo;

    /* interrupt mode receive */
	/* disable interrupt */
	level = rt_hw_interrupt_disable();
    rx_fifo = uart2_rx_fifo;
    while (1)
    {
        ch = ls1c_uart_getc();
        if (ch == -1) 
			break;



        rx_fifo->buffer[rx_fifo->put_index] = ch;
        rx_fifo->put_index += 1;
        if (rx_fifo->put_index >= UART_BUF_SIZE) 
			rx_fifo->put_index = 0;

        /* if the next position is read index, discard this 'read char' */
        if (rx_fifo->put_index == rx_fifo->get_index)
        {
            rx_fifo->get_index += 1;
            rx_fifo->is_full = 1;
            if (rx_fifo->get_index >= UART_BUF_SIZE) 
				rx_fifo->get_index = 0;
        }

        /* enable interrupt */
    }
	rt_hw_interrupt_enable(level);
}

static void uart_irq_handler(int vector, void *param)
{
	
	//register long level = rt_hw_interrupt_disable();
	struct rt_uart_ls1c *uart_dev = &uart2;

	void *uart_base = uart_get_base(uart_dev->UARTx);
	unsigned char iir = reg_read_8(uart_base + LS1C_UART_IIR_OFFSET);

	// 判断是否为接收超时或接收到有效数据
	if ((IIR_RXTOUT & iir) || (IIR_RXRDY & iir))
	{
		rt_hw_serial_isr();
	}
	//rt_hw_interrupt_enable(level);
}

irq_handler_t rt_hw_interrupt_install(int vector, irq_handler_t handler,
										void *param, const char *name)
{
   irq_handler_t old_handler = NULL;

   if (vector >= 0 && vector < MAX_INTR)
   {
	   old_handler = irq_handlers[vector].handler;

	   irq_handlers[vector].handler = handler;
	   irq_handlers[vector].param = param;
   }

   return old_handler;
}

static long ls1c_uart_configure()
{
	struct rt_uart_ls1c *uart_dev =  &uart2;
	ls1c_uart_info_t uart_info = {0};

	// 初始化串口
	uart_info.UARTx    = uart_dev->UARTx;
	uart_info.baudrate = 115200;
	uart_info.rx_enable = 1;
	uart_init(&uart_info);
	return 0;
}

rt_inline int _serial_int_rx(unsigned char *data, int length)//从缓存中获取一个字符
{
    int size;
    struct rt_serial_rx_fifo* rx_fifo=uart2_rx_fifo;
    size = length;
    /* read from software FIFO */
    while (length)
    {
        int ch;
        long level;

        /* disable interrupt */
        level = rt_hw_interrupt_disable();

        /* there's no data: */
        if ((rx_fifo->get_index == rx_fifo->put_index) && (rx_fifo->is_full == 0))
        {
            /* no data, enable interrupt and break out */
            rt_hw_interrupt_enable(level);
            break;
        }

        /* otherwise there's the data: */
        ch = rx_fifo->buffer[rx_fifo->get_index];
        rx_fifo->get_index += 1;
        if (rx_fifo->get_index >= UART_BUF_SIZE) 
			rx_fifo->get_index = 0;

        if (rx_fifo->is_full == 1)
        {
            rx_fifo->is_full = 0;
        }

        /* enable interrupt */
        rt_hw_interrupt_enable(level);

        *data = ch & 0xff;
        data ++; length --;
    }

    return size - length;
}

int getchar(void)
{
    char ch = 0;
    while (_serial_int_rx(&ch, 1)!=1)//rt_device_read(NULL, -1, &ch, 1) != 1
    {
        /*
        ret=_serial_int_rx(NULL,&ch, 1);
		if(ret==1)
			break;
		*/
    }
    return (int)ch;
}

void uart2GetCharInit(void)//这个可能与裸机程序串口初始化重复了 但是无所谓啦
{
	struct rt_uart_ls1c *uart= &uart2;

	uart2_rx_fifo=(struct rt_serial_rx_fifo*) malloc (sizeof(struct rt_serial_rx_fifo) +UART_BUF_SIZE);
	uart2_rx_fifo->buffer = (unsigned char*) (uart2_rx_fifo + 1);
	memset(uart2_rx_fifo->buffer, 0, UART_BUF_SIZE);
	uart2_rx_fifo->put_index = 0;
	uart2_rx_fifo->get_index = 0;
	uart2_rx_fifo->is_full = 0;
				
	rt_hw_interrupt_install(uart->IRQ, uart_irq_handler,NULL, "UART2");//
	
	ls1c_uart_configure();
	rt_hw_interrupt_umask(uart->IRQ);//开启中断
	//rt_hw_interrupt_mask(uart->IRQ);//关闭中断
	

}



