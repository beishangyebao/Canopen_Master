#include "ds402_master.h"

#include "can_utils.h"
#include "delay.h"
#include <stdio.h>

/*
 * DS402 主站状态机辅助模块
 * 本文件把 0x6040 Controlword 的写入动作和 0x6041 Statusword 的等待确认集中管理
 * 上层运动模式只负责选择 6060 模式和下发目标值，避免各处重复盲发 6040
 */

/* CAN 接收中断在解析 TPDO1 后更新该缓存，这里只读取最近一次状态字 */
extern UNS16 recv_statusword;

/*
 * 函数功能：按 DS402 标准推荐的 mask/value 方式匹配状态字
 * 输入参数：
 *   sw    - 从站 0x6041 Statusword 原始值
 *   mask  - 参与状态判断的位掩码
 *   value - mask 后期望得到的标准状态值
 * 返回值：
 *   1 表示匹配；0 表示不匹配
 * 说明：
 *   不能直接完整等值比较 0x6041，因为从站会叠加电压、remote、target reached
 *   homing attained/error 等模式相关位，这些位不应影响 PDS 主状态判断
 */
static int ds402_master_status_match(uint16_t sw, uint16_t mask, uint16_t value)
{
    return ((sw & mask) == value);
}

/*
 * 函数功能：向从站 0x6040 Controlword 写入一个控制字
 * 输入参数：
 *   node        - CANopen 从站节点号
 *   controlword - 准备写入 0x6040:00 的控制字
 *   tag         - 串口日志中显示的动作名称
 * 返回值：
 *   0 表示 SDO 写成功；非 0 表示写请求、超时或 abort 失败
 * 说明：
 *   所有 DS402 控制动作都通过这个函数进入，便于在串口日志里看到
 *   shutdown、switch on、enable operation 等动作具体写了哪个控制字
 */
static int ds402_master_write_controlword(uint8_t node, uint16_t controlword, const char *tag)
{
    int ret;

    /* 0x6040 是 DS402 控制字，固定使用 16 位 SDO 下载 */
    ret = canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, (uint32_t)controlword, 2);
    if (ret != 0) {
        printf("[DS402] %s failed, node=%u cw=0x%04X ret=%d\r\n",
               tag, node, (unsigned)controlword, ret);
        return ret;
    }

    printf("[DS402] %s -> node=%u cw=0x%04X\r\n",
           tag, node, (unsigned)controlword);
    return 0;
}

/*
 * 函数功能：读取状态字中的单个位
 * 输入参数：
 *   sw  - 从站 0x6041 Statusword 原始值
 *   bit - 要读取的 bit 序号，范围 0~15
 * 返回值：
 *   1 表示该位为 1；0 表示该位为 0
 * 说明：
 *   status 命令会打印 bit4、bit5、bit9、bit10、bit12、bit13
 *   这些位用于判断电压、快速停止、远程控制、目标到达和回零结果
 */
static uint8_t ds402_master_status_bit(uint16_t sw, uint8_t bit)
{
    return (uint8_t)((sw >> bit) & 0x0001u);
}

/*
 * 函数功能：把 DS402 状态字解析为主站内部枚举
 * 输入参数：
 *   sw - 从站 0x6041 Statusword 原始值，通常来自 TPDO1 缓存 recv_statusword
 * 返回值：
 *   Ds402MasterState 枚举值，表示当前 PDS 主状态
 * 说明：
 *   判断顺序按 DS402 PDS finite state automaton 常用 mask 表组织
 *   可兼容从站叠加模式相关位后的状态字
 */
Ds402MasterState ds402_master_decode_status(uint16_t sw)
{
    if (ds402_master_status_match(sw, 0x004Fu, 0x0000u)) {
        return DS402_MASTER_STATE_NOT_READY_TO_SWITCH_ON;
    }
    if (ds402_master_status_match(sw, 0x004Fu, 0x0040u)) {
        return DS402_MASTER_STATE_SWITCH_ON_DISABLED;
    }
    if (ds402_master_status_match(sw, 0x006Fu, 0x0021u)) {
        return DS402_MASTER_STATE_READY_TO_SWITCH_ON;
    }
    if (ds402_master_status_match(sw, 0x006Fu, 0x0023u)) {
        return DS402_MASTER_STATE_SWITCHED_ON;
    }
    if (ds402_master_status_match(sw, 0x006Fu, 0x0027u)) {
        return DS402_MASTER_STATE_OPERATION_ENABLED;
    }
    if (ds402_master_status_match(sw, 0x006Fu, 0x0007u)) {
        return DS402_MASTER_STATE_QUICK_STOP_ACTIVE;
    }
    if (ds402_master_status_match(sw, 0x004Fu, 0x000Fu)) {
        return DS402_MASTER_STATE_FAULT_REACTION_ACTIVE;
    }
    if (ds402_master_status_match(sw, 0x004Fu, 0x0008u)) {
        return DS402_MASTER_STATE_FAULT;
    }

    return DS402_MASTER_STATE_UNKNOWN;
}

/*
 * 函数功能：把 DS402 状态枚举转换为可读字符串
 * 输入参数：
 *   state - ds402_master_decode_status() 得到的状态枚举
 * 返回值：
 *   指向常量字符串的指针，用于串口打印
 * 说明：
 *   所有等待超时和 status 命令都使用统一名称，便于现场日志对照文档分析
 */
const char *ds402_master_state_name(Ds402MasterState state)
{
    switch (state) {
    case DS402_MASTER_STATE_NOT_READY_TO_SWITCH_ON:
        return "Not ready to switch on";

    case DS402_MASTER_STATE_SWITCH_ON_DISABLED:
        return "Switch on disabled";

    case DS402_MASTER_STATE_READY_TO_SWITCH_ON:
        return "Ready to switch on";

    case DS402_MASTER_STATE_SWITCHED_ON:
        return "Switched on";

    case DS402_MASTER_STATE_OPERATION_ENABLED:
        return "Operation enabled";

    case DS402_MASTER_STATE_QUICK_STOP_ACTIVE:
        return "Quick stop active";

    case DS402_MASTER_STATE_FAULT_REACTION_ACTIVE:
        return "Fault reaction active";

    case DS402_MASTER_STATE_FAULT:
        return "Fault";

    default:
        return "Unknown";
    }
}

/*
 * 函数功能：在限定时间内等待从站进入指定 DS402 主状态
 * 输入参数：
 *   expected   - 期望进入的 DS402 主状态
 *   timeout_ms - 等待超时时间，单位 ms
 * 返回值：
 *   0 表示已等到目标状态；-1 表示超时
 * 说明：
 *   状态来源是 TPDO1 接收缓存 recv_statusword，不主动 SDO 读取 0x6041
 *   CAN 接收中断会持续刷新该缓存，因此这里每 2ms 解码一次即可
 */
int ds402_master_wait_state(Ds402MasterState expected, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;

    while (elapsed < timeout_ms) {
        Ds402MasterState current = ds402_master_decode_status((uint16_t)recv_statusword);

        /* 只要 TPDO 缓存显示已经到达目标状态，就立即返回给上层 */
        if (current == expected) {
            return 0;
        }

        delay_ms(2);
        elapsed += 2;
    }

    printf("[DS402] wait %s timeout, sw=0x%04X state=%s\r\n",
           ds402_master_state_name(expected),
           (unsigned)recv_statusword,
           ds402_master_state_name(ds402_master_decode_status((uint16_t)recv_statusword)));
    return -1;
}

/*
 * 函数功能：执行 DS402 fault reset 上升沿并等待回到 Switch on disabled
 * 输入参数：
 *   node - CANopen 从站节点号
 * 返回值：
 *   0 表示故障复位后状态回到 Switch on disabled；非 0 表示写控制字或等待失败
 * 说明：
 *   DS402 从站通常只识别 0x6040 bit7 的上升沿，所以这里按
 *   0x0000 -> 0x0080 -> 0x0000 产生复位脉冲，而不是长期保持 bit7
 */
int ds402_master_fault_reset(uint8_t node)
{
    if (ds402_master_write_controlword(node, 0x0000u, "fault reset low") != 0) {
        return -1;
    }
    delay_ms(5);

    if (ds402_master_write_controlword(node, 0x0080u, "fault reset pulse") != 0) {
        return -1;
    }
    delay_ms(5);

    if (ds402_master_write_controlword(node, 0x0000u, "fault reset release") != 0) {
        return -1;
    }

    return ds402_master_wait_state(DS402_MASTER_STATE_SWITCH_ON_DISABLED, 1000);
}

/*
 * 函数功能：写 disable voltage 控制字并等待 Switch on disabled
 * 输入参数：
 *   node - CANopen 从站节点号
 * 返回值：
 *   0 表示从站确认进入 Switch on disabled；非 0 表示写入或等待失败
 * 说明：
 *   该动作对应 0x6040=0x0000，主要用于现场调试时主动撤销电压使能
 */
int ds402_master_disable_voltage(uint8_t node)
{
    if (ds402_master_write_controlword(node, 0x0000u, "disable voltage") != 0) {
        return -1;
    }

    return ds402_master_wait_state(DS402_MASTER_STATE_SWITCH_ON_DISABLED, 500);
}

/*
 * 函数功能：写 shutdown 控制字并等待 Ready to switch on
 * 输入参数：
 *   node - CANopen 从站节点号
 * 返回值：
 *   0 表示从站确认进入 Ready to switch on；非 0 表示写入或等待失败
 * 说明：
 *   该动作对应 0x6040=0x0006，是标准使能流程的第一步
 */
int ds402_master_shutdown(uint8_t node)
{
    if (ds402_master_write_controlword(node, 0x0006u, "shutdown") != 0) {
        return -1;
    }

    return ds402_master_wait_state(DS402_MASTER_STATE_READY_TO_SWITCH_ON, 500);
}

/*
 * 函数功能：写 quick stop 控制字并等待 Quick stop active
 * 输入参数：
 *   node - CANopen 从站节点号
 * 返回值：
 *   0 表示从站确认进入 Quick stop active；非 0 表示写入或等待失败
 * 说明：
 *   该动作对应 0x6040=0x0002。若从站策略是在停稳后自动跳转到其他状态
 *   超时日志会显示最终状态，便于判断从站 quick stop 策略
 */
int ds402_master_quick_stop(uint8_t node)
{
    if (ds402_master_write_controlword(node, 0x0002u, "quick stop") != 0) {
        return -1;
    }

    return ds402_master_wait_state(DS402_MASTER_STATE_QUICK_STOP_ACTIVE, 500);
}

/*
 * 函数功能：按 DS402 标准三段式流程使能到 Operation enabled
 * 输入参数：
 *   node - CANopen 从站节点号
 * 返回值：
 *   0 表示完成 Operation enabled；非 0 表示任一步控制字或状态等待失败
 * 说明：
 *   本函数是速度、位置、回零、力矩四类运动命令共用的使能入口
 *   它会先处理 Fault / Fault reaction active，再按
 *   shutdown -> switch on -> enable operation 逐步等待状态确认
 */
int ds402_master_enable_operation(uint8_t node)
{
    Ds402MasterState state;

    /* NMT start 后 TPDO 可能稍晚到达，先留出很短时间让状态缓存刷新 */
    delay_ms(5);
    state = ds402_master_decode_status((uint16_t)recv_statusword);

    if (state == DS402_MASTER_STATE_FAULT) {
        if (ds402_master_fault_reset(node) != 0) {
            return -1;
        }
    } else if (state == DS402_MASTER_STATE_FAULT_REACTION_ACTIVE) {
        if (ds402_master_wait_state(DS402_MASTER_STATE_FAULT, 1000) != 0) {
            return -1;
        }
        if (ds402_master_fault_reset(node) != 0) {
            return -1;
        }
    }

    state = ds402_master_decode_status((uint16_t)recv_statusword);
    if (state == DS402_MASTER_STATE_NOT_READY_TO_SWITCH_ON) {
        if (ds402_master_wait_state(DS402_MASTER_STATE_SWITCH_ON_DISABLED, 1000) != 0) {
            return -1;
        }
    }

    if (ds402_master_write_controlword(node, 0x0006u, "shutdown") != 0) {
        return -1;
    }
    if (ds402_master_wait_state(DS402_MASTER_STATE_READY_TO_SWITCH_ON, 500) != 0) {
        return -1;
    }

    if (ds402_master_write_controlword(node, 0x0007u, "switch on") != 0) {
        return -1;
    }
    if (ds402_master_wait_state(DS402_MASTER_STATE_SWITCHED_ON, 500) != 0) {
        return -1;
    }

    if (ds402_master_write_controlword(node, 0x000Fu, "enable operation") != 0) {
        return -1;
    }
    if (ds402_master_wait_state(DS402_MASTER_STATE_OPERATION_ENABLED, 500) != 0) {
        return -1;
    }

    return 0;
}

/*
 * 函数功能：判断当前 TPDO 缓存是否处于 Operation enabled
 * 输入参数：
 *   无
 * 返回值：
 *   1 表示当前状态是 Operation enabled；0 表示不是
 * 说明：
 *   连续位置 cp/cr、连续力矩 ct、halt 等命令会调用本函数做安全门槛
 *   避免在从站未使能时直接发送运动触发或 halt 控制字
 */
int ds402_master_is_operation_enabled(void)
{
    return (ds402_master_decode_status((uint16_t)recv_statusword) ==
            DS402_MASTER_STATE_OPERATION_ENABLED);
}

/*
 * 函数功能：打印 DS402 状态字主状态和关键附加位
 * 输入参数：
 *   sw - 从站 0x6041 Statusword 原始值
 * 返回值：
 *   无
 * 说明：
 *   status 命令调用本函数，让调试者不用手工拆 bit 即可判断
 *   是否电压已使能、是否 remote、是否 target reached、回零是否完成或报错
 */
void ds402_master_print_status(uint16_t sw)
{
    Ds402MasterState state = ds402_master_decode_status(sw);

    printf("状态字: 0x%04X (%s)\r\n",
           (unsigned)sw, ds402_master_state_name(state));
    printf("bit4 Voltage enabled: %u\r\n", (unsigned)ds402_master_status_bit(sw, 4));
    printf("bit5 Quick stop: %u\r\n", (unsigned)ds402_master_status_bit(sw, 5));
    printf("bit9 Remote: %u\r\n", (unsigned)ds402_master_status_bit(sw, 9));
    printf("bit10 Target reached: %u\r\n", (unsigned)ds402_master_status_bit(sw, 10));
    printf("bit12 Mode specific / Homing attained: %u\r\n",
           (unsigned)ds402_master_status_bit(sw, 12));
    printf("bit13 Mode specific / Homing error: %u\r\n",
           (unsigned)ds402_master_status_bit(sw, 13));
}



