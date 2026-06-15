
#include "can_rxlog.h"
#include <stdio.h>

#if CAN_RXLOG_ENABLE  == 1

// 用环形队列存储接收到的CAN帧
typedef struct {
    UNS16 cob_id;   // CAN帧ID
    UNS8  rtr;      // 远程帧标志
    UNS8  len;      // 数据长度
    UNS8  data[8];  // 数据内容
} RxLogItem;

static volatile uint16_t widx = 0;   // 写指针 指示下一个空位在哪里
static volatile uint16_t ridx = 0;   // 读指针 指示下一个要处理的数据在哪里
static RxLogItem q[CAN_RXLOG_QSIZE]; // 环形队列


// 计算下一个索引位置，如果超出范围则循环到开头
static  uint16_t next_idx(uint16_t i)
{ 
    uint16_t next;

    next = i + 1;

    // 如果超出了数组最大长度，就回到 0 (形成了环)
    if (next >= CAN_RXLOG_QSIZE)
    {
        next = 0;
    }
    return next;
}

// 中断调用，把接收到的CAN帧数据写入队列
void can_rxlog_push_isr(const Message *m)
{
    uint16_t n = next_idx(widx);

    if(n == ridx){  //读指针追上写指针，队列满
        return; // 队列满：直接丢弃，避免阻塞中断
    }
    //把 CAN 帧数据写入当前写位置
    q[widx].cob_id = m->cob_id;
    q[widx].rtr    = m->rtr;
    q[widx].len    = m->len;

    // 复制数据
    memcpy(q[widx].data, m->data, 8);

    //移动写指针
    widx = n;
}


// 从队列取出数据：成功返回1，失败返回0
int can_rxlog_pop(Message *m)
{
    if (m == 0) {
        return 0;
    }

    // 队列空
    if (ridx == widx) {
        return 0;
    }

    // 取当前读指针的数据
    m->cob_id = q[ridx].cob_id;
    m->rtr    = q[ridx].rtr;
    m->len    = q[ridx].len;

    // 复制数据
    memcpy(m->data, q[ridx].data, 8);

    // 移动读指针
    ridx = next_idx(ridx);

    return 1;
}


void can_rxlog_poll_print(void)
{
   Message m;  // 创建一个临时变量来接数据

    // 从环形队列中取出所有待处理的CAN消息
    while (can_rxlog_pop(&m))
    {
        // 统一走过滤器打印
        canlog_on_rx_frame_print(&m);
    }
}

#else

void can_rxlog_push_isr(const Message *m){ (void)m; }
void can_rxlog_poll_print(void){}

#endif
