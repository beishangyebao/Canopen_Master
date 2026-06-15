#include "can_log.h"

#include <stdio.h>

/*
 * CAN 报文日志模块。
 * 该文件只负责“是否打印”和“如何打印”两件事：
 * 1. 按 COB-ID 推断报文类别。
 * 2. 根据总开关、单一 COB 过滤和黑名单过滤决定是否输出。
 * 3. 将接收帧内容以串口文本形式打印出来，便于调试总线行为。
 */

/* 黑名单可容纳的 COB-ID 数量上限。 */
#define ID_FILTER_MAX 16

/* 整体日志输出开关，1 表示允许输出。 */
static uint8_t g_log_enable = 1;

/* only 过滤器。非 0 时，仅允许该 COB-ID 被打印。 */
static uint16_t g_log_only_cob = 0;

/* 各类报文的独立开关，索引与 LogClass 枚举保持一致。 */
static uint8_t g_log_class_enable[LOG_CLASS_MAX] = {
    0,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    1
};

/* 黑名单表，用于屏蔽指定的 COB-ID 输出。 */
static uint16_t g_log_id_blk[ID_FILTER_MAX];

/* 当前黑名单中已经登记的项数。 */
static uint8_t g_log_id_blk_n = 0;

/*
 * 根据 11 位标准 COB-ID 的功能段判断报文类型。
 * 这里使用掩码 0x780 提取功能码区域，再映射到日志分类。
 */
static LogClass classify_cob(uint16_t cob)
{
    /* 0x700 ~ 0x77F: 心跳/节点守护。 */
    if ((cob & 0x780) == 0x700) {
        return LOG_HB;
    }

    /* 0x180/280/380/480 段: TPDO1~4。 */
    if ((cob & 0x780) == 0x180 ||
        (cob & 0x780) == 0x280 ||
        (cob & 0x780) == 0x380 ||
        (cob & 0x780) == 0x480) {
        return LOG_TPDO;
    }

    /* 0x200/300/400/500 段: RPDO1~4。 */
    if ((cob & 0x780) == 0x200 ||
        (cob & 0x780) == 0x300 ||
        (cob & 0x780) == 0x400 ||
        (cob & 0x780) == 0x500) {
        return LOG_RPDO;
    }

    /* 0x580 ~ 0x5FF: 从站返回给主站的 SDO 响应。 */
    if ((cob & 0x780) == 0x580) {
        return LOG_SDO_RX;
    }

    /* 0x600 ~ 0x67F: 主站发给从站的 SDO 请求。 */
    if ((cob & 0x780) == 0x600) {
        return LOG_SDO_TX;
    }

    /* 0x000: NMT 主站控制报文。 */
    if (cob == 0x000) {
        return LOG_NMT;
    }

    /* 0x080: SYNC 同步报文。 */
    if (cob == 0x080) {
        return LOG_SYNC;
    }

    /* 0x081 ~ 0x0FF: EMCY 紧急报文。 */
    if ((cob & 0x780) == 0x080 && cob != 0x080) {
        return LOG_EMCY;
    }

    /* 其余未归类报文统一归入 OTHER。 */
    return LOG_OTHER;
}

/* 将枚举类别转换为短字符串，供打印使用。 */
static const char *logclass_name(LogClass c)
{
    switch (c) {
        case LOG_HB:     return "HB";
        case LOG_TPDO:   return "TPDO";
        case LOG_RPDO:   return "RPDO";
        case LOG_SDO_RX: return "SDO-RX";
        case LOG_SDO_TX: return "SDO-TX";
        case LOG_NMT:    return "NMT";
        case LOG_SYNC:   return "SYNC";
        case LOG_EMCY:   return "EMCY";
        default:         return "OTHER";
    }
}

/*
 * 在黑名单表中查找目标 COB-ID。
 * 找到时返回数组下标，未找到返回 -1。
 */
static int idblk_find(uint16_t cob)
{
    uint8_t i;

    for (i = 0; i < g_log_id_blk_n; i++) {
        if (g_log_id_blk[i] == cob) {
            return (int)i;
        }
    }
    return -1;
}

/* 判断指定 COB-ID 是否命中黑名单。 */
static uint8_t idblk_hit(uint16_t cob)
{
    return (idblk_find(cob) >= 0);
}

/* 设置总日志开关。任何非 0 输入都会被视为开启。 */
void canlog_enable(uint8_t en)
{
    if (en == 0) {
        g_log_enable = 0;
    } else {
        g_log_enable = 1;
    }
}

/* 设置 only 过滤器。传入 0 表示取消单一 COB-ID 限制。 */
void canlog_only_set(uint16_t cob)
{
    g_log_only_cob = cob;
}

/* 按类别启用或关闭日志输出。 */
void canlog_class_enable(LogClass cls, uint8_t en)
{
    if (cls >= LOG_CLASS_MAX) {
        return;
    }

    if (en == 0) {
        g_log_class_enable[cls] = 0;
    } else {
        g_log_class_enable[cls] = 1;
    }
}

/*
 * 将指定 COB-ID 加入黑名单。
 * 该函数只在未超出容量时追加，不负责扩容。
 */
void canlog_blk_add(uint16_t cob)
{
    if (idblk_find(cob) == 1) {
        return;
    }

    if (g_log_id_blk_n < ID_FILTER_MAX) {
        g_log_id_blk[g_log_id_blk_n] = cob;
        g_log_id_blk_n += 1;
    }
}

/*
 * 从黑名单中移除指定 COB-ID。
 * 删除后会将尾部元素前移，保持数组连续。
 */
void canlog_blk_del(uint16_t cob)
{
    int idx = idblk_find(cob);
    uint8_t i;

    if (idx < 0) {
        return;
    }

    for (i = (uint8_t)idx; i + 1 < g_log_id_blk_n; i++) {
        g_log_id_blk[i] = g_log_id_blk[i + 1];
    }

    g_log_id_blk_n--;
}

/* 打印当前日志配置，便于串口查看过滤器状态。 */
void canlog_status_print(void)
{
    uint8_t i;

    printf("=== CANLOG STATUS ===\r\n");
    printf("enable=%u\r\n", (unsigned)g_log_enable);
    printf("only=0x%03X\r\n", (unsigned)g_log_only_cob);

    printf("class: HB=%u TPDO=%u RPDO=%u SDO_RX=%u SDO_TX=%u NMT=%u SYNC=%u EMCY=%u OTHER=%u\r\n",
           g_log_class_enable[LOG_HB],
           g_log_class_enable[LOG_TPDO],
           g_log_class_enable[LOG_RPDO],
           g_log_class_enable[LOG_SDO_RX],
           g_log_class_enable[LOG_SDO_TX],
           g_log_class_enable[LOG_NMT],
           g_log_class_enable[LOG_SYNC],
           g_log_class_enable[LOG_EMCY],
           g_log_class_enable[LOG_OTHER]);

    printf("blk list (%u/%u): \r\n", (unsigned)g_log_id_blk_n, (unsigned)ID_FILTER_MAX);

    for (i = 0; i < g_log_id_blk_n; i++) {
        printf("0x%03X ", (unsigned)g_log_id_blk[i]);
    }
}

/*
 * 统一决策当前报文是否允许打印。
 * 检查顺序依次为：总开关 -> only 过滤 -> 黑名单 -> 类别开关。
 */
static uint8_t canlog_should_print(uint16_t cob)
{
    LogClass cls;

    if (g_log_enable == 0) {
        return 0;
    }

    if (g_log_only_cob && cob != g_log_only_cob) {
        return 0;
    }

    if (idblk_hit(cob) != 0) {
        return 0;
    }

    cls = classify_cob(cob);
    return g_log_class_enable[cls];
}

/*
 * 打印一帧接收到的标准 CAN 数据帧。
 * 仅处理普通数据帧，RTR 帧会被直接忽略。
 */
void canlog_on_rx_frame_print(const Message *m)
{
    uint16_t cob;
    LogClass cls;
    uint8_t i;

    if (m == NULL) {
        return;
    }

    if (m->rtr == 1) {
        return;
    }

    cob = (uint16_t)m->cob_id;
    if (!canlog_should_print(cob)) {
        return;
    }

    cls = classify_cob(cob);

    printf("[RX][%s] COB=0x%03X dlc=%u : \r\n", logclass_name(cls), (unsigned)cob, (unsigned)m->len);

    /* 逐字节输出负载内容，便于和对象字典字段做人工比对。 */
    for (i = 0; i < m->len; i++) {
        printf("%02X  ", m->data[i]);
    }

    printf("\r\n");
}
