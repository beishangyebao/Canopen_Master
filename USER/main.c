#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "can.h"
#include "canfestival.h"
#include "canfestival_can.h"
#include "canfestival_timer.h"
#include "canfestival_master.h"
#include "can_rxlog.h"
#include "cmd_uart.h"
#include <stdio.h>

/*
 * 主控入口文件。
 * 负责完成 MCU 基础初始化、CANFestival 主站启动、串口命令处理和
 * 接收报文日志轮询，是整个调试程序的顶层调度入口。
 */

/* 串口驱动中对外暴露的初始化接口。 */
void usart_Init(u32 baudrate);

/* 标记系统启动完成的外部钩子函数。 */
void markStartupDone(void);

int main(void)
{
    /* 串口命令接收缓冲区，一次最多解析 127 个可见字符。 */
    char uart_buf[128];

    /* 配置中断优先级分组，为后续外设中断初始化打基础。 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* 初始化系统时钟和 1ms SysTick 节拍。 */
    SystemInit();
    SysTick_Config(SystemCoreClock / 1000);

    /* 初始化延时模块，供 SDO/PDO 时序等待使用。 */
    delay_init();

    /* 启动 TIM3 周期中断，通常用于协议定时或软件时基。 */
    TIM3_Int_Init(4999, 71);

    /* 初始化调试串口，命令解析和日志输出均通过该通道完成。 */
    usart_Init(115200);

    /* 初始化 CAN 控制器并设置位时序。 */
    canfestival_can_init(CAN_SJW_1tq, CAN_BS2_5tq, CAN_BS1_6tq, 3, CAN_Mode_Normal);

    /*
     * 配置本机为主站节点并推进协议栈状态机：
     * Initialisation -> Pre-operational -> Operational。
     */
    setNodeId(&Master_Data, 0x7F);
    setState(&Master_Data, Initialisation);
    setState(&Master_Data, Pre_operational);
    setState(&Master_Data, Operational);

    /* 通知外部模块，主站启动流程已经结束。 */
    markStartupDone();

    /* 留出短暂稳定时间，避免刚启动就立刻发送控制命令。 */
    delay_ms(50);

    /*
     * 保留原有启动提示字符串，不改动其字面内容，只补充注释说明用途。
     * 该打印用于串口观察主站是否已走完整个初始化流程。
     */
    printf("主站启动成功\r\n");

    while (1) {
        /* 有完整串口命令到达时交给命令解析模块处理。 */
        if (USART_Gets(uart_buf, sizeof(uart_buf))) {
            parse_uart_cmd(uart_buf);
        }

        /* 轮询 CAN 接收日志输出，避免在主循环中丢失调试信息。 */
        can_rxlog_poll_print();
    }
}
