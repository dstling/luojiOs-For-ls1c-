龙芯1C裸机库+多线程+shell

既然用裸机库，咱们以后就叫它luojisos吧^_^
=====================================================
20191024更新日志
新代码更改原裸机库较多，不再赘述

1、修复thread.c内多处bug
2、更新完善net驱动，现在比较稳定了
3、lwip移植比较完善了
4、可以使用tftp，通过网络更新luojios.bin了
5、可以通过tftp使用load和go了，目前测试load rtthread.elf正常
6、新增了一些设置功能，将参数保存在rom中(可以set mac地址 ip地址等)
7、写个《luoji设置和指令说明》吧


之前记录
=====================================================

闲来无事，给裸机加上多线程和shell
风格完全是裸机化，方便学习和追踪
luojiOs bug基本消除了，可以完美运行了

裸机增加多线程和shell交互是有用的，方便调试，调试完成后屏蔽多线程入口，加入纯裸接口即可
多线程的缺点就是降低了实时性，尤其是应用于工控领域
在多线程中，严格按照流程执行的程序会存在一些不确定性

裸机代码来自  https://gitee.com/caogos/OpenLoongsonLib1c
多线程来自rtthread  https://github.com/RT-Thread/rt-thread
rtthread是完美的、伟大的，致敬！（没有严格按照rtt的调度来移植）
rtthread的官方doc：https://www.rt-thread.org/document/site/um4003-rtthread-programming-manual.pdf
rtthread 龙芯1C的部分默认用的编译器是mips-sde-elf

而裸机OS这里使用的编译器是官方的gcc-4.3-ls232，这里提供下
环境在虚拟机中设置
export PATH=$PATH:$PATH:/home/opt/gcc-4.3-ls232/bin/（虚拟机内编译器解压的绝对路径）
cd /home/luojiN/OpenLoongsonLib1c20190726/src/ (该代码在虚拟机中的绝对路径)
make

=============================
白菜板内存8M 直接可以编译运行
如果为了节省rom直接，请把os/net文件夹删除即可
=============================

智龙板内存32M且有网卡 
1、可以修改文件sdram_cfg .h 42行if1改为if 0 
2、malloc.c init_heaptop函数堆顶可以设置为0x82000000 32M
3、初始化网卡shell输入rt_hw_eth_init即可
 ip地址在os/net/lwip-1.4.1/arch/include/lwipopts.h中设置
用你的电脑ping玩吧，最终目的是用这个os实现pmon的bootloader功能
哈哈
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
    ......
// 先将指定pin的所有复用清零
    for (i=0; i<=4; i++)
    {
        reg = (volatile unsigned int *)(LS1C_CBUS_FIRST0 + (port*0x04) + (i*0x10));
        reg_clr_one_bit(reg, pin);          // 清零
    }
	
    if (remap == PIN_REMAP_DEFAULT) return;//增加的 dstliing

    switch (port)
....
}

7、start.S内修改了多处，有关异常和中断的处理改到os/context_gcc.S中的mips_irq_handle
8、ls1c_irq.c 修改一处
8、malloc移植至smalllib，rtthread的malloc更好,malloc_align函数申请的内存暂时不能free, (DMA内存申请才会用到,而且申请了一般也不会free)





