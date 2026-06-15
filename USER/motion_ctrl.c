#include "motion_ctrl.h"
#include "can_utils.h"
#include "delay.h"
#include "ds402_master.h"
#include <stdio.h>

/*
 * 运动控制辅助模块。
 * 该文件封装了几种常用的 CiA402 控制方式：
 * 1. 速度模式 PV / CV。
 * 2. 位置模式 PP。
 * 3. 回零模式 Homing。
 * 4. 力矩模式 PT / CT。
 * 上层命令解析模块只需调用这些接口，无需重复拼接 SDO/PDO 内容。
 */

/* 当前工程固定使用的 RPDO1 / RPDO2 COB-ID。 */
#define RPDO1_COBID 0x201
#define RPDO2_COBID 0x301

/*
 * 通过 RPDO1 下发位置模式控制字、模式字节和目标位置。
 * node 参数当前未参与 COB-ID 计算，保留是为了接口语义完整。
 */
static void rpdo1_send_pp(uint8_t node, uint16_t cw, int8_t mode, int32_t pos)
{
    uint8_t buf[7];

    (void)node;
    can_utils_put_u16_le(&buf[0], cw);
    buf[2] = (uint8_t)mode;
    can_utils_put_u32_le(&buf[3], (uint32_t)pos);

    can_utils_send_ext(RPDO1_COBID, buf, 7);

    printf("[RPDO1-PP] CW=0x%04X Mode=%d Pos=%ld -> COB-ID=0x%03X\r\n",
           cw, (int)mode, (long)pos, RPDO1_COBID);
}

/*
 * 通过 RPDO2 下发速度模式目标速度。
 * 后两个字节补 0，匹配当前从站映射格式。
 */
static void rpdo2_send_pv(uint8_t node, int32_t vel)
{
    uint8_t buf[6];

    (void)node;
    can_utils_put_u32_le(&buf[0], (uint32_t)vel);
    can_utils_put_u16_le(&buf[4], 0);

    can_utils_send_ext(RPDO2_COBID, buf, 6);

    printf("[RPDO2-PV] Vel=%ld -> COB-ID=0x%03X\r\n",
           (long)vel, RPDO2_COBID);
}

/*
 * 通过 RPDO2 下发目标力矩。
 * 前 4 字节填 0，仅在后两字节携带目标力矩值。
 */
static void rpdo2_send_pt(uint8_t node, int16_t torque)
{
    uint8_t buf[6];

    (void)node;
    can_utils_put_u32_le(&buf[0], 0);
    can_utils_put_u16_le(&buf[4], (uint16_t)torque);

    can_utils_send_ext(RPDO2_COBID, buf, 6);

    printf("[RPDO2-PT] Torque=%d -> COB-ID=0x%03X\r\n",
           (int)torque, RPDO2_COBID);
}

/*
 * 函数功能：进入 Profile Velocity 转速模式并通过 RPDO2 下发目标速度。
 * 输入参数：
 *   node  - CANopen 从站节点号。
 *   speed - 目标速度，单位由从站 0x60FF/速度环定义决定。
 * 返回值：
 *   无。
 * 工作流程：
 *   1. 通过 SDO 写 0x6060=3，通知从站切换到 Profile Velocity 模式。
 *   2. 调用 DS402 主站 helper，按 6040/6041 状态机逐步使能到 Operation enabled。
 *   3. 只有确认使能成功后，才通过 RPDO2 把目标速度送给从站。
 */
void motion_pv_run(uint8_t node, int32_t speed)
{
    /* 速度模式选择失败时直接返回，避免在未知模式下继续使能运动。 */
    if (canopen_send_sdo(node, 0x6060, 0x00, ODT_I8, 0x03, 1) != 0) {
        return;
    }

    /* 统一 DS402 使能流程会处理故障复位、上电初始化等待和三段式控制字。 */
    if (ds402_master_enable_operation(node) != 0) {
        printf("[PV] enable operation failed, node=%u\r\n", node);
        return;
    }

    /* 从站已确认 Operation enabled 后，再发送真正会改变运动状态的目标速度。 */
    rpdo2_send_pv(node, speed);

    printf("[PV] node=%u speed=%ld via RPDO2(0x%03X)\r\n",
           node, (long)speed, RPDO2_COBID);
}

/*
 * 函数功能：在已处于速度模式时，通过 RPDO2 实时更新目标速度。
 * 输入参数：
 *   node  - CANopen 从站节点号。
 *   speed - 新目标速度。
 * 返回值：
 *   无。
 * 说明：
 *   连续速度命令不再隐式执行使能流程，只允许在 Operation enabled 下改速。
 */
void motion_cv_set_speed(uint8_t node, int32_t speed)
{
    /* 连续改速只做目标值更新，若状态机未使能则拒绝发送运动目标。 */
    if (!ds402_master_is_operation_enabled()) {
        printf("[CV] not operation enabled\r\n");
        return;
    }

    rpdo2_send_pv(node, speed);
}

/*
 * 函数功能：进入 Profile Position 位置模式并完成 DS402 使能。
 * 输入参数：
 *   node - CANopen 从站节点号。
 * 返回值：
 *   无。
 * 工作流程：
 *   1. 通过 SDO 写 0x6060=1，选择 PP 位置模式。
 *   2. 调用 ds402_master_enable_operation()，等待 6041 依次确认标准状态。
 *   3. 本函数只完成模式准备和使能，不产生 new set-point 脉冲。
 */
void motion_pp_init_enable(uint8_t node)
{
    /* 位置模式选择失败时不继续执行使能，避免后续位置触发落在错误模式。 */
    if (canopen_send_sdo(node, 0x6060, 0x00, ODT_I8, 0x01, 1) != 0) {
        return;
    }

    if (ds402_master_enable_operation(node) != 0) {
        printf("[PP] enable operation failed, node=%u\r\n", node);
        return;
    }

    printf("[PP] Node %u Mode=1 (Position) Enabled.\r\n", node);
}

/*
 * 触发一次位置模式运动。
 * bit5 用于 new set-point 流程准备，bit6 区分绝对/相对运动，
 * bit4 通过“清零 -> 置位 -> 清零”的脉冲方式触发执行。
 */
static void pp_move_trigger(uint8_t node, int32_t target, uint8_t is_relative)
{
    uint16_t cw_base = 0x000F;
    uint16_t cw_clear;
    uint16_t cw_set;

    /* PP 连续位置命令必须由调用者先完成 DS402 使能，否则 new set-point 无效且危险。 */
    if (!ds402_master_is_operation_enabled()) {
        printf("[PP] not operation enabled\r\n");
        return;
    }

    /* bit5=change set immediately；bit6=relative/absolute；bit4=new set-point。 */
    cw_base |= (1 << 5);
    if (is_relative) {
        cw_base |= (1 << 6);
    }

    cw_clear = cw_base & ~(1 << 4);
    cw_set = cw_base | (1 << 4);

    rpdo1_send_pp(node, cw_clear, 0x01, target);
    delay_ms(2);

    rpdo1_send_pp(node, cw_set, 0x01, target);
    delay_ms(2);

    rpdo1_send_pp(node, cw_clear, 0x01, target);

    printf("[PP] Node %u Move %s -> %ld (via RPDO1)\r\n",
           node, is_relative ? "REL" : "ABS", (long)target);
}

/* 执行绝对位置运动。 */
void motion_pp_move_abs(uint8_t node, int32_t target_abs)
{
    pp_move_trigger(node, target_abs, 0);
}

/* 执行相对位置运动。 */
void motion_pp_move_rel(uint8_t node, int32_t distance)
{
    pp_move_trigger(node, distance, 1);
}

/*
 * 函数功能：进入 Homing 回零模式并启动一次回零动作。
 * 输入参数：
 *   node   - CANopen 从站节点号。
 *   method - 写入 0x6098 的回零方法编号。
 * 返回值：
 *   无。
 * 工作流程：
 *   1. 写 0x6060=6 选择 Homing 模式。
 *   2. 写 0x6098 设置回零方法。
 *   3. 调用统一 DS402 使能 helper，确认 Operation enabled。
 *   4. 写 0x6040=0x001F，利用 bit4 启动 homing operation。
 */
void motion_homing_run(uint8_t node, int8_t method)
{
    if (canopen_send_sdo(node, 0x6060, 0x00, ODT_I8, 0x06, 1)) {
        return;
    }
    if (canopen_send_sdo(node, 0x6098, 0x00, ODT_I8, (uint32_t)(uint8_t)method, 1)) {
        return;
    }

    /* 回零也必须先走标准 DS402 使能流程，不能直接盲发启动控制字。 */
    if (ds402_master_enable_operation(node) != 0) {
        printf("[Homing] enable operation failed, node=%u\r\n", node);
        return;
    }

    canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, 0x001F, 2);

    printf("[Homing] Node %u Method %d Started.\r\n", node, method);
}

/*
 * 函数功能：切换到 Profile Torque 转矩模式并完成 DS402 使能。
 * 输入参数：
 *   node - CANopen 从站节点号。
 * 返回值：
 *   0 表示模式选择和 Operation enabled 均成功；-1 表示失败。
 * 说明：
 *   该函数只准备力矩模式，不下发目标力矩，目标值由 motion_pt_run()/motion_ct_set_torque() 发送。
 */
static int pt_init_enable(uint8_t node)
{
    if (canopen_send_sdo(node, 0x6060, 0x00, ODT_I8, 0x04, 1) != 0) {
        return -1;
    }

    if (ds402_master_enable_operation(node) != 0) {
        printf("[PT] enable operation failed, node=%u\r\n", node);
        return -1;
    }

    printf("[PT] Node %u Mode=4 (Torque) Enabled.\r\n", node);
    return 0;
}

/*
 * 力矩模式运行入口。
 * 先发 NMT start，再完成模式使能，最后通过 RPDO2 下发目标力矩。
 */
void motion_pt_run(uint8_t node, int16_t torque)
{
    sendNMT(node, 0x01);
    delay_ms(5);

    /* 力矩目标只在模式和 DS402 使能都成功后发送。 */
    if (pt_init_enable(node) != 0) {
        return;
    }

    rpdo2_send_pt(node, torque);
}

/*
 * 函数功能：在已处于力矩模式时，通过 RPDO2 实时更新目标力矩。
 * 输入参数：
 *   node   - CANopen 从站节点号。
 *   torque - 新目标力矩，比例定义由从站 0x6071/力矩环决定。
 * 返回值：
 *   无。
 * 说明：
 *   连续转矩命令不做隐式使能，必须先由 pt 命令或其他流程进入 Operation enabled。
 */
void motion_ct_set_torque(uint8_t node, int16_t torque)
{
    /* 从站未使能时拒绝直接更新目标力矩，避免状态机和运动目标脱节。 */
    if (!ds402_master_is_operation_enabled()) {
        printf("[CT] not operation enabled\r\n");
        return;
    }

    rpdo2_send_pt(node, torque);
}
