#include "canfestival_timer.h"

//通用定时器3中断初始化
//这里时钟选择为APB1的2倍，而APB1为36M
//arr：自动重装值,系统计数器从0计数到arr后溢出，产生中断
//psc：时钟预分频数
//这里使用的是定时器3通用定时器，支持通用计数、PWM、输入捕获、输出比较
/*STM32定时器时钟 = 72MHz（APB1 是 36MHz，被自动乘2）
  实际定时频率 = 72MHz / (psc + 1)
  溢出周期 = (arr + 1) / 实际定时频率
*/
void TIM3_Int_Init(u16 arr,u16 psc)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); //时钟使能
	
	//定时器TIM3初始化
	TIM_TimeBaseStructure.TIM_Period = arr; //设置在下一个更新事件装入活动的自动重装载寄存器周期的值	
	TIM_TimeBaseStructure.TIM_Prescaler =psc; //设置用来作为TIMx时钟频率除数的预分频值
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; //设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM向上计数模式
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); //根据指定的参数初始化TIM3的时间基数单位
 
	TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE ); //使能指定的TIM3中断,允许更新中断

	//中断优先级NVIC设置
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;  //TIM3中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  //先占优先级0级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;  //从优先级1级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQ通道使能

	NVIC_Init(&NVIC_InitStructure);  //初始化NVIC寄存器

	TIM_Cmd(TIM3, ENABLE);  //使能TIM3计时器开始计数					 
}


void TIM3_IRQHandler(void)   //定时器3中断处理函数
{
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)  //检查TIM3更新中断发生与否
		{
			TimeDispatch(); // 调用TimeDispatch函数处理定时器事件
		    TIM_ClearITPendingBit(TIM3, TIM_IT_Update  );  //清除中断标志           
		}
}


/**
 * @brief 设置定时器的下一次中断触发时间
 * @param value 设置的时间间隔值，表示下一次中断在多少时间后触发
 */
void setTimer(TIMEVAL value)
{
    // 设置下一次自动重装载寄存器的值
    // 计算方法为当前计数值(CNT)加上设定的间隔值(value)
    // 这样可以保证下一次中断在 value 时间之后触发
	TIM3->ARR = TIM3->CNT + value;  //下一次中断在 value 时间之后再触发
	                                //由最近需要唤醒的定时器决定
}

TIMEVAL getElapsedTime(void)
{
	return TIM3->CNT;    //计算从上次setTimer调用到当前时刻的时间间隔即overrun
}


/*	STM32 72 MHz ──┬─ TIM3 预分频 → 1 MHz ──┬─ 每 1 计数 = 1 ?0?8s
               │                        │
               │                        ├─ ARR 可变 → 中断间隔可变
               │                        │
               │                        └→ TIM3_IRQHandler 每进一次
               │                           → TimeDispatch() 更新所有软定时器
               │
               └→ getElapsedTime() 读 TIM3->CNT 做误差补偿
*/
