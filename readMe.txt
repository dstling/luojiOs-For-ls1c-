===================================================
20191203更新日志（智龙板，白菜板可以试一下多线程的效果哈）

从现在开始，其实这个bootloader（我这里就叫luojiOs了）完全想利用rtthread的多线程思想，想去实现pmon的load节奏
但是在load linux内核时，存在着性能严重下降，效率极其低下的问题，我也在分析为什么会这样
我仔细的分析了为什么会性能下降这么严重（除了异常处理和多线程外，实在想不出原因）
在os/filesys/new_go.c中 完全模仿了pmon的引导模式 无果
分析的结果，个人猜想，pmon没有使用严格的多线程（pmon没有多线程，没有初始化中断等，但是能回到从前，使用了setjmp和termio的回到main函数之前定位的断点位置，），
不知道哪个地方初始化出了问题，也无从（其实我也严格的考证了pmon的setjmp等原理）考证为什么性能低下了，我感觉应该是模式性能降低了
从linux内核估算的性能结果，下降了大约（250/7）35倍，哇咔咔咔，哭死中
希望大神能够给予帮助
load rtthread.elf看起来一切正常，why？rtthread重新初始化了异常和CPU cp0寄存器等信息？linux内核干了啥？

主要更新日志
1、编译前修改os/include/buildconfig.h内的ip地址等信息
2、net驱动完全可以使用了，尤其是tftp
3、usb驱动可以使用了，u盘格式fat32，load方式设置loadelfAddr为 /dev/usbX(X为tree识别的盘符号) 
4、智龙板子的nand驱动已经完善了，完全可以和pmon一样load了（性能低下，原因不明）

做了很多，有兴趣的可以一起探讨下，pmon分析来分析去，还是很好用啊，哈哈

===================================================

20191024更新日志
龙芯1C裸机库+多线程+shell+load elf
继续造轮子,代码杂糅了rtt、pmon、裸机库
既然用裸机库，就叫它luojisos吧^_^
新代码更改原裸机库较多，不再赘述

1、编译前修改os/include/buildconfig.h内的ip地址等信息
2、修复thread.c内多处bug
3、更新完善net驱动，现在比较稳定了
4、lwip移植比较完善了
5、可以使用tftp，通过网络更新luojios.bin了
6、可以通过tftp使用load和go了，目前测试load 
rtthread.elf正常（提供了一个带开发板nand文件系统的rtthread.elf文件）
7、新增了一些设置功能，将参数保存在rom中(可以set mac地址 ip地址等)
8、写个《luoji设置和指令说明》吧



之前历史记录
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





