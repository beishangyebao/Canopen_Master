#include "can_utils.h"

#include "delay.h"
#include "can.h"
#include "canfestival_master.h"
#include <stdio.h>
#include <string.h>

/*
 * CANOpen 通讯辅助模块。
 * 这里封装了几类通用能力：
 * 1. 小端字节序打包，便于组织 PDO 数据区。
 * 2. 直接发送标准帧 PDO/NMT 报文。
 * 3. 基于 CanFestival 主站接口执行 SDO 读写并打印结果。
 */

/* 将 16 位无符号值按小端格式写入目标缓冲区。 */
void can_utils_put_u16_le(uint8_t *d, uint16_t v)
{
    d[0] = (uint8_t)(v & 0xFF);
    d[1] = (uint8_t)((v >> 8) & 0xFF);
}

/* 将 32 位无符号值按小端格式写入目标缓冲区。 */
void can_utils_put_u32_le(uint8_t *d, uint32_t v)
{
    d[0] = (uint8_t)(v & 0xFF);
    d[1] = (uint8_t)((v >> 8) & 0xFF);
    d[2] = (uint8_t)((v >> 16) & 0xFF);
    d[3] = (uint8_t)((v >> 24) & 0xFF);
}

/*
 * 发送一帧标准 11 位 CAN 数据帧。
 * 参数 cob_id 只保留低 11 位，长度超过 8 字节时会被截断。
 */
int can_utils_send_ext(UNS32 cob_id, const uint8_t *data, uint8_t len)
{
    Message msg;

    if (len > 8) {
        len = 8;
    }

    msg.cob_id = (UNS16)(cob_id & 0x7FF);
    msg.rtr = NOT_A_REQUEST;
    msg.len = len;
    memcpy(msg.data, data, len);

    if (canSend(Master_Data.canHandle, &msg) != 0) {
        return -1;
    }

    return 0;
}

/* 发送 NMT 主站状态切换命令，并把发送结果打印到串口。 */
void sendNMT(uint8_t node_id, uint8_t command)
{
    UNS8 ret = masterSendNMTstateChange(&Master_Data, node_id, command);

    if (ret == 0) {
        printf("[NMT] Sent -> Cmd=0x%02X Node=%u\r\n", command, node_id);
    } else {
        printf("[NMT] send failed, ret=%u (Cmd=0x%02X Node=%u)\r\n", ret, command, node_id);
    }
}

/*
 * 执行一次 SDO 写操作。
 * 处理流程如下：
 * 1. 把 32 位入参裁剪到 data_size 指定的有效字节数。
 * 2. 关闭旧的 SDO 事务，避免上一次传输残留状态。
 * 3. 发起写请求并轮询完成状态。
 * 4. 超时或收到 abort code 时给出明确错误码。
 */
int canopen_send_sdo(uint8_t node_id, uint16_t index, uint8_t subindex,
                     uint8_t data_type, uint32_t data, uint8_t data_size)
{
    uint8_t sdo_data[4] = {0};
    UNS8 res;
    UNS32 abort_code = 0;
    UNS32 timer_cnt = 0;

    /* 按原始内存布局复制低位字节，适配 1/2/4 字节写入。 */
    memcpy(sdo_data, &data, data_size);

    /* 开始新事务前，先显式关闭可能残留的客户端传输状态。 */
    closeSDOtransfer(&Master_Data, node_id, SDO_CLIENT);

    res = writeNetworkDict(&Master_Data, node_id, index, subindex,
                           data_size, data_type, sdo_data, 0);

    if (res != 0) {
        printf("[SDO] Write Request Failed: %u\r\n", res);
        return -1;
    }

    /* 轮询等待下载完成，超时时主动终止事务。 */
    while (getWriteResultNetworkDict(&Master_Data, node_id, &abort_code) == SDO_DOWNLOAD_IN_PROGRESS) {
        delay_ms(2);
        timer_cnt += 2;
        if (timer_cnt > SDO_TIMEOUT_MS) {
            closeSDOtransfer(&Master_Data, node_id, SDO_CLIENT);
            printf("[SDO] Write Timeout: Node %d Idx 0x%04X\r\n", node_id, index);
            return -3;
        }
    }

    /* 无论成功失败，都在结束时关闭本次客户端传输。 */
    closeSDOtransfer(&Master_Data, node_id, SDO_CLIENT);

    if (abort_code != 0) {
        printf("[SDO] Write Abort: 0x%lX\r\n", abort_code);
        return -2;
    }

    return 0;
}

/*
 * 执行一次 SDO 读操作，并直接将返回值打印出来。
 * 该函数更偏向调试用途，因此结果不会返回给上层，而是直接串口输出。
 */
void canopen_read_sdo_print(uint8_t node_id, uint16_t index, uint8_t subindex)
{
    uint8_t buf[8];
    UNS32 size = sizeof(buf);
    UNS32 abort_code = 0;
    UNS32 timer_cnt = 0;
    UNS32 i;
    unsigned long long val = 0;

    /* 关闭旧事务，确保本次上传从干净状态开始。 */
    closeSDOtransfer(&Master_Data, node_id, SDO_CLIENT);

    if (readNetworkDict(&Master_Data, node_id, index, subindex, 0, NULL) != 0) {
        printf("Read Req Fail\r\n");
        return;
    }

    /* 周期轮询上传结果，直到完成、超时或收到 abort。 */
    while (getReadResultNetworkDict(&Master_Data, node_id, buf, &size, &abort_code) == SDO_UPLOAD_IN_PROGRESS) {
        delay_ms(2);
        timer_cnt += 2;
        if (timer_cnt > SDO_TIMEOUT_MS) {
            closeSDOtransfer(&Master_Data, node_id, SDO_CLIENT);
            printf("Read Timeout\r\n");
            return;
        }
    }

    closeSDOtransfer(&Master_Data, node_id, SDO_CLIENT);

    if (abort_code != 0) {
        printf("Read Abort: 0x%lX\r\n", abort_code);
    } else {
        /* 将最多 8 字节返回值按小端重组，便于统一打印。 */
        for (i = 0; i < size && i < 8; ++i) {
            val |= ((unsigned long long)buf[i]) << (8 * i);
        }
        printf("Val: 0x%llX (size=%lu)\r\n", val, (unsigned long)size);
    }
}
