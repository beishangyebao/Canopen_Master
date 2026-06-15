#include "cmd_uart.h"

#include "can_log.h"
#include "can_utils.h"
#include "delay.h"
#include "ds402_master.h"
#include "motion_ctrl.h"
#include <stdio.h>
#include <string.h>

/*
 * 串口命令解析模块。
 * 所有文本命令都从这里进入，再分发到 CANOpen 读写、运动控制和日志配置接口。
 * 设计目标是调试方便，因此命令格式尽量直接对应对象字典、模式号和目标值。
 */

/* 由接收日志模块维护的从站实时反馈量，这里只负责查询打印。 */
extern UNS16 recv_statusword;
extern INTEGER32 recv_actual_vel;
extern INTEGER32 recv_actual_pos;
extern INTEGER16 recv_actual_torque;

/*
 * 解析一条完整串口命令。
 * 匹配顺序基本按功能类别排列：SDO/PDO -> NMT -> 运动控制 -> 状态查询 -> 日志配置。
 */
void parse_uart_cmd(char *cmd)
{
    /* sdor: 读取指定对象字典项，并直接打印返回值。 */
    if (strncmp(cmd, "sdor", 4) == 0) {
        uint8_t node;
        uint8_t sub;
        uint16_t index;

        if (sscanf(cmd, "sdor %hhu %hx %hhx", &node, &index, &sub) == 3) {
            canopen_read_sdo_print(node, index, sub);
        } else {
            printf("Err: sdor <node> <idx> <sub>\r\n");
        }
        return;
    } else if (strncmp(cmd, "sdo ", 4) == 0) {
        /* sdo: 写指定对象字典项，size 决定使用 1/2/4 字节写入。 */
        uint8_t node;
        uint8_t sub;
        uint8_t size;
        uint16_t index;
        uint32_t val;

        if (sscanf(cmd, "sdo %hhu %hx %hhx %hhu %lx", &node, &index, &sub, &size, &val) == 5) {
            uint8_t dtype = (size == 1) ? ODT_U8 : (size == 2) ? ODT_U16 : ODT_U32;
            canopen_send_sdo(node, index, sub, dtype, val, size);
        } else {
            printf("Err: sdo <node> <idx> <sub> <sz> <val>\r\n");
        }
        return;
    } else if (strncmp(cmd, "pdo", 3) == 0) {
        /*
         * pdo: 预留的原始 PDO 命令入口。
         * 当前代码仅做参数解析占位，未继续下发实际报文。
         */
        uint8_t node;
        uint8_t pdo_num;
        uint8_t d[8];
        int n;

        n = sscanf(cmd, "pdo %hhu %hhu %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx",
                   &node, &pdo_num, &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7]);
        if (n >= 4) {
            printf("用法: pdo <node> <pdo#1-4> <b0> [b1..b7]\r\n");
        }
        return;
    } else if (strncmp(cmd, "nmt", 3) == 0) {
        /* nmt: 用文本动作名映射标准 NMT 控制字。 */
        uint8_t node;
        char action[20];

        if (sscanf(cmd, "nmt %hhu %19s", &node, action) == 2) {
            uint8_t cs;

            if (strcmp(action, "start") == 0) {
                cs = 0x01;
            } else if (strcmp(action, "stop") == 0) {
                cs = 0x02;
            } else if (strcmp(action, "preop") == 0) {
                cs = 0x80;
            } else if (strcmp(action, "reset") == 0) {
                cs = 0x81;
            } else if (strcmp(action, "resetcom") == 0) {
                cs = 0x82;
            } else {
                printf("未知 NMT 命令: %s\r\n", action);
                return;
            }
            sendNMT(node, cs);
        } else {
            printf("用法: nmt <node> <start|stop|preop|reset|resetcom>\r\n");
        }
        return;
    } else if (strncmp(cmd, "pv ", 3) == 0) {
        /* pv: 先发送 NMT start，再切到速度模式并运行。 */
        uint8_t node;
        long spd;

        if (sscanf(cmd, "pv %hhu %ld", &node, &spd) == 2) {
            sendNMT(node, 0x01);
            motion_pv_run(node, (int32_t)spd);
        } else {
            printf("用法: pv <node> <speed>\r\n");
        }
        return;
    } else if (strncmp(cmd, "cv ", 3) == 0) {
        /* cv: 默认认为当前已经处于速度控制可直接改速。 */
        uint8_t node;
        long rpm;

        if (sscanf(cmd, "cv %hhu %ld", &node, &rpm) == 2) {
            motion_cv_set_speed(node, (int32_t)rpm);
        } else {
            printf("用法: cv <node> <rpm>\r\n");
        }
        return;
    } else if (strncmp(cmd, "pp ", 3) == 0) {
        /* pp: 绝对位置模式完整启动流程，可选附带轮廓速度。 */
        uint8_t node;
        long pos;
        long speed = 0;
        int n = sscanf(cmd, "pp %hhu %ld %ld", &node, &pos, &speed);

        if (n < 2) {
            printf("用法: pp <node> <pos> [speed]\r\n");
            return;
        }
        if (n == 3) {
            canopen_send_sdo(node, 0x6081, 0x00, ODT_U32, (uint32_t)speed, 4);
            delay_ms(5);
        }

        sendNMT(node, 0x01);
        motion_pp_init_enable(node);
        motion_pp_move_abs(node, (int32_t)pos);
        return;
    } else if (strncmp(cmd, "cp ", 3) == 0) {
        /* cp: 连续绝对位置命令，默认设备已使能。 */
        uint8_t node;
        long pos;
        long speed = 0;
        int n = sscanf(cmd, "cp %hhu %ld %ld", &node, &pos, &speed);

        if (n < 2) {
            printf("用法: cp <node> <pos> [speed]\r\n");
            return;
        }
        if (n == 3) {
            canopen_send_sdo(node, 0x6081, 0x00, ODT_U32, (uint32_t)speed, 4);
            delay_ms(5);
        }
        motion_pp_move_abs(node, (int32_t)pos);
        return;
    } else if (strncmp(cmd, "pr ", 3) == 0) {
        /* pr: 相对位置模式完整启动流程，可选附带轮廓速度。 */
        uint8_t node;
        long delta;
        long speed = 0;
        int n = sscanf(cmd, "pr %hhu %ld %ld", &node, &delta, &speed);

        if (n < 2) {
            printf("用法: pr <node> <delta> [speed]\r\n");
            return;
        }
        if (n == 3) {
            canopen_send_sdo(node, 0x6081, 0x00, ODT_U32, (uint32_t)speed, 4);
            delay_ms(5);
        }

        sendNMT(node, 0x01);
        motion_pp_init_enable(node);
        motion_pp_move_rel(node, (int32_t)delta);
        return;
    } else if (strncmp(cmd, "cr ", 3) == 0) {
        /* cr: 连续相对位置命令，默认设备已完成位置模式准备。 */
        uint8_t node;
        long delta;
        long speed = 0;
        int n = sscanf(cmd, "cr %hhu %ld %ld", &node, &delta, &speed);

        if (n < 2) {
            printf("用法: cr <node> <delta> [speed]\r\n");
            return;
        }
        if (n == 3) {
            canopen_send_sdo(node, 0x6081, 0x00, ODT_U32, (uint32_t)speed, 4);
            delay_ms(5);
        }

        motion_pp_move_rel(node, (int32_t)delta);
        return;
    } else if (strncmp(cmd, "hm ", 3) == 0) {
        /* hm: 启动一次回零。 */
        uint8_t node;
        int method;

        if (sscanf(cmd, "hm %hhu %d", &node, &method) == 2) {
            sendNMT(node, 0x01);
            motion_homing_run(node, (int8_t)method);
        } else {
            printf("用法: hm <node> <method>\r\n");
        }
        return;
    } else if (strncmp(cmd, "pt ", 3) == 0) {
        /* pt: 力矩模式启动并下发目标力矩。 */
        uint8_t node;
        long trq;

        if (sscanf(cmd, "pt %hhu %ld", &node, &trq) == 2) {
            motion_pt_run(node, (int16_t)trq);
        } else {
            printf("用法: pt <node> <torque (1000=100%%)>\r\n");
        }
        return;
    } else if (strncmp(cmd, "ct ", 3) == 0) {
        /* ct: 已进入力矩控制后，直接更新目标力矩。 */
        uint8_t node;
        long trq;

        if (sscanf(cmd, "ct %hhu %ld", &node, &trq) == 2) {
            motion_ct_set_torque(node, (int16_t)trq);
        } else {
            printf("用法: ct <node> <torque>\r\n");
        }
        return;
    } else if (strncmp(cmd, "faultreset", 10) == 0) {
        /* faultreset: 产生 DS402 bit7 上升沿，用于从 Fault 回到 Switch on disabled。 */
        uint8_t node;

        if (sscanf(cmd, "faultreset %hhu", &node) == 1) {
            ds402_master_fault_reset(node);
        } else {
            printf("用法: faultreset <node>\r\n");
        }
        return;
    } else if (strncmp(cmd, "disable", 7) == 0) {
        /* disable: 写 6040=0x0000，撤销电压使能并等待 Switch on disabled。 */
        uint8_t node;

        if (sscanf(cmd, "disable %hhu", &node) == 1) {
            ds402_master_disable_voltage(node);
        } else {
            printf("用法: disable <node>\r\n");
        }
        return;
    } else if (strncmp(cmd, "shutdown", 8) == 0) {
        /* shutdown: 写 6040=0x0006，等待 Ready to switch on。 */
        uint8_t node;

        if (sscanf(cmd, "shutdown %hhu", &node) == 1) {
            ds402_master_shutdown(node);
        } else {
            printf("用法: shutdown <node>\r\n");
        }
        return;
    } else if (strncmp(cmd, "status", 6) == 0) {
        /* status: 打印最近一次 TPDO 缓存，并把 6041 解析为 DS402 主状态和关键 bit。 */
        printf("=== 从站反馈 ===\r\n");
        ds402_master_print_status((uint16_t)recv_statusword);
        printf("实际速度: %ld\r\n", (long)recv_actual_vel);
        printf("实际位置: %ld\r\n", (long)recv_actual_pos);
        printf("实际力矩: %d\r\n", (int)recv_actual_torque);
        return;
    } else if (strncmp(cmd, "halt", 4) == 0) {
        /* halt: 只允许在 Operation enabled 下发送带 halt 位的控制字。未指定节点时默认 1。 */
        uint8_t node = 1;

        sscanf(cmd, "halt %hhu", &node);
        if (!ds402_master_is_operation_enabled()) {
            printf("[HALT] not operation enabled\r\n");
            return;
        }
        canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, 0x010F, 2);
        return;
    } else if (strncmp(cmd, "qstop", 5) == 0) {
        /* qstop: 发送快速停止控制字并等待 Quick stop active。未指定节点时默认 1。 */
        uint8_t node = 1;

        sscanf(cmd, "qstop %hhu", &node);
        ds402_master_quick_stop(node);
        return;
    } else if (strncmp(cmd, "log", 3) == 0) {
        /*
         * log: 调试输出配置命令族。
         * 支持查看状态、总开关、only 过滤、指定 ID 屏蔽和分类开关。
         */
        char a[16] = {0};
        char b[16] = {0};
        unsigned id = 0;

        if (sscanf(cmd, "log %15s", a) == 1 && !strcmp(a, "status")) {
            canlog_status_print();
            return;
        }

        if (sscanf(cmd, "log %15s", a) == 1) {
            if (!strcmp(a, "on")) {
                canlog_enable(1);
                printf("log on\r\n");
                return;
            }
            if (!strcmp(a, "off")) {
                canlog_enable(0);
                printf("log off\r\n");
                return;
            }
        }

        if (sscanf(cmd, "log only %x", &id) == 1) {
            canlog_only_set((uint16_t)id);
            printf("log only = 0x%03X\r\n", (unsigned)((uint16_t)id));
            return;
        }

        if (sscanf(cmd, "log id %x %15s", &id, a) == 2) {
            uint16_t cob = (uint16_t)id;

            if (!strcmp(a, "off")) {
                canlog_blk_add(cob);
                printf("log id 0x%03X off\r\n", (unsigned)cob);
            } else if (!strcmp(a, "on")) {
                canlog_blk_del(cob);
                printf("log id 0x%03X on\r\n", (unsigned)cob);
            } else {
                printf("Usage: log id <cob_hex> <on|off>\r\n");
            }
            return;
        }

        if (sscanf(cmd, "log %15s %15s", a, b) == 2) {
            uint8_t en = (!strcmp(b, "on")) ? 1 : (!strcmp(b, "off")) ? 0 : 2;

            if (en == 2) {
                printf("Usage: log <hb|sdo|tpdo|rpdo|other> <on|off>\r\n");
                return;
            }
            if (!strcmp(a, "hb")) {
                canlog_class_enable(LOG_HB, en);
                printf("log hb %s\r\n", b);
                return;
            }
            if (!strcmp(a, "sdo")) {
                canlog_class_enable(LOG_SDO_RX, en);
                canlog_class_enable(LOG_SDO_TX, en);
                printf("log sdo %s\r\n", b);
                return;
            }
            if (!strcmp(a, "tpdo")) {
                canlog_class_enable(LOG_TPDO, en);
                printf("log tpdo %s\r\n", b);
                return;
            }
            if (!strcmp(a, "rpdo")) {
                canlog_class_enable(LOG_RPDO, en);
                printf("log rpdo %s\r\n", b);
                return;
            }
            if (!strcmp(a, "other")) {
                canlog_class_enable(LOG_OTHER, en);
                printf("log other %s\r\n", b);
                return;
            }

            printf("Unknown log class\r\n");
            return;
        }

        return;
    }

    /* 未匹配到任何命令时，统一给出默认错误提示。 */
    printf("未知命令\r\n");
}
