// 硬件定时器源码


#include "ls1c_public.h"
#include "ls1c_pin.h"
#include "ls1c_clock.h"
#include "ls1c_regs.h"
#include "ls1c_pwm.h"
#include "ls1c_timer.h"


// 定时器中计数器(CNTR、HRC和LRC)的最大值
#define TIMER_COUNTER_MAX               (0xffffff)



/*
 * 获取指定定时器的寄存器基地址
 * @timer 硬件定时器
 * @ret 基地址
 */
unsigned int timer_get_reg_base(ls1c_timer_t timer)
{
    unsigned int reg_base = 0;

    switch (timer)
    {
        case TIMER_PWM0:
            reg_base = LS1C_REG_BASE_PWM0;
            break;

        case TIMER_PWM1:
            reg_base = LS1C_REG_BASE_PWM1;
            break;

        case TIMER_PWM2:
            reg_base = LS1C_REG_BASE_PWM2;
            break;

        case TIMER_PWM3:
            reg_base = LS1C_REG_BASE_PWM3;
            break;
    }

    return reg_base;
}


/*
 * 初始化定时器，并开始定时
 * @timer_info 定时器和定时时间信息
 */
void timer_init(timer_info_t *timer_info)
{
    unsigned int timer_reg_base = 0;        // 寄存器基地址
    unsigned long timer_clk = 0;            // 硬件定时器的时钟
    unsigned long tmp;
    unsigned int ctrl = 0;                  // 控制寄存器中的控制信息
    
    // 判断入参
    if (NULL == timer_info)
    {
        return ;
    }

    /*
     * 把定时时间换算为计数器的值
     * 计数器值 = 定时器的时钟 * 定时时间(单位ns) / 1000000000
     * 龙芯1c的定时器时钟为APB时钟，达到126Mhz，
     * 为避免计算过程发生溢出，这里采用手动优化上面的计算式，也可以采用浮点运算
     */
    timer_clk = clk_get_apb_rate();
    tmp = (timer_clk / 1000000) * (timer_info->time_ns / 1000);     // 将1000000000拆分为1000000和1000
    tmp = MIN(tmp, TIMER_COUNTER_MAX);

    // 控制寄存器信息
    ctrl = (1 << LS1C_PWM_INT_LRC_EN)
           | (0 << LS1C_PWM_INT_HRC_EN)
           | (0 << LS1C_PWM_CNTR_RST)
           | (0 << LS1C_PWM_INT_SR)
           | (1 << LS1C_PWM_INTEN)
           | (0 << LS1C_PWM_SINGLE)
           | (1 << LS1C_PWM_OE)
           | (1 << LS1C_PWM_CNT_EN);

    // 设置各个寄存器
    timer_reg_base = timer_get_reg_base(timer_info->timer);     // 获取寄存器基地址
    reg_write_32(0,                     (volatile unsigned int *)(timer_reg_base + LS1C_PWM_HRC));
    reg_write_32(tmp--,                 (volatile unsigned int *)(timer_reg_base + LS1C_PWM_LRC));
    reg_write_32(0,                     (volatile unsigned int *)(timer_reg_base + LS1C_PWM_CNTR));
    reg_write_32(ctrl,                  (volatile unsigned int *)(timer_reg_base + LS1C_PWM_CTRL));

    return ;
}



/*
 * 停止定时器
 * @timer_info 定时器
 */
void timer_stop(timer_info_t *timer_info)
{
    unsigned int timer_reg_base = 0;
    
    // 判断入参
    if (NULL == timer_info)
    {
        return ;
    }

    timer_reg_base = timer_get_reg_base(timer_info->timer);
    reg_write_32(0, (volatile unsigned int *)(timer_reg_base + LS1C_PWM_CTRL));

    return ;
}


/*
 * 重启定时器
 * @timer_info 定时器
 */
void inline timer_restart(timer_info_t *timer_info)
{
	unsigned int timer_reg_base = 0;
	unsigned int ctrl = 0;

    ctrl = (1 << LS1C_PWM_INT_LRC_EN)
           | (0 << LS1C_PWM_INT_HRC_EN)
           | (0 << LS1C_PWM_CNTR_RST)
           | (0 << LS1C_PWM_INT_SR)
           | (1 << LS1C_PWM_INTEN)
           | (0 << LS1C_PWM_SINGLE)
           | (1 << LS1C_PWM_OE)
           | (1 << LS1C_PWM_CNT_EN);

	timer_reg_base = timer_get_reg_base(timer_info->timer);
	reg_write_32(ctrl, (volatile unsigned int *)(timer_reg_base + LS1C_PWM_CTRL));

	return ;
}


/*
 * 打印timer相关寄存器的值
 * @timer_info 硬件定时器
 */
void timer_print_regs(timer_info_t *timer_info)
{
    unsigned int timer_reg_base = 0;

    timer_reg_base = timer_get_reg_base(timer_info->timer);
    printf("CNTR=0x%x, HRC=0x%x, LRC=0x%x, CTRL=0x%x\n",
              reg_read_32((volatile unsigned int *)(timer_reg_base + LS1C_PWM_CNTR)),
              reg_read_32((volatile unsigned int *)(timer_reg_base + LS1C_PWM_HRC)),
              reg_read_32((volatile unsigned int *)(timer_reg_base + LS1C_PWM_LRC)),
              reg_read_32((volatile unsigned int *)(timer_reg_base + LS1C_PWM_CTRL)));

    return ;
}


