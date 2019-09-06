闲来无事，给裸机加上多线程和shell，代码抄自rtthread
风格完全变成裸机风格，现在多线程能够好好的运行了
裸机增加多线程和shell是有用的，方便调试，调试完成后删除多线程即可
这里移植的多线程的缺点就是降低了实时性，严格按照流程执行的程序会存在一些不确定性
没有严格按照rtt的调度来移植

=============================
白菜板内存8M 直接可以编译运行
如果为了节省rom直接把net文件夹删除了

=============================
智龙板内存32M且有网卡 
1、可以修改文件sdram_cfg .h 42行if1改为if 0 
2、malloc.c init_heaptop函数堆顶可以设置为0x82000000 32M
3、初始化网卡shell输入rt_hw_eth_init即可
 ip地址在os/net/lwip-1.4.1/arch/include/lwipopts.h中设置
用你的电脑ping玩吧
============================


裸机库的代码修改了多处：
1、lib/ls1c_uart.c中
printf打印换行无需\r\n，一个\n即可
2、printf函数中buf增加关键字static
2、修改了根目录下的ld.script
3、修改了makefile
4、修改了lib/ls1c_sys_tick.c sys_tick_handler函数中加入threadTask();
5、修改了lib/ls1c_pin.h 
typedef enum
{
    PIN_REMAP_FIRST = 0,                // 第一复用
    PIN_REMAP_SECOND,                   // 第二复用
    PIN_REMAP_THIRD,                    // 第三复用
    PIN_REMAP_FOURTH,                   // 第四复用
    PIN_REMAP_FIFTH,                    // 第五复用
    PIN_REMAP_DEFAULT,                //缺省复用 dstling
}pin_remap_t;
6、lib/ls1c_pin.c
void pin_set_remap(unsigned int gpio, pin_remap_t remap)
{
    volatile unsigned int *reg = NULL;          // 复用寄存器
    unsigned int port = GPIO_GET_PORT(gpio);
    unsigned int pin  = GPIO_GET_PIN(gpio);
    int i;

    // 先将指定pin的所有复用清零
    for (i=0; i<=4; i++)
    {
        reg = (volatile unsigned int *)(LS1C_CBUS_FIRST0 + (port*0x04) + (i*0x10));
        reg_clr_one_bit(reg, pin);          // 清零
    }
	
    if (remap == PIN_REMAP_DEFAULT) return;//增加的 dstliing

    switch (port)
    {
。。。。。。。


7、start.S内修改了多处
8、ls1c_irq.c 修改一处
8、malloc移植至smalllib，rtthread的malloc更好,malloc_align函数申请的内存暂时不能free, (DMA内存申请才会用到,而且申请了一般也不会free)





