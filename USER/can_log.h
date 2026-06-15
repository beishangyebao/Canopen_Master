#ifndef CAN_LOG_H
#define CAN_LOG_H

#include <stdint.h>
#include "canfestival.h"

typedef enum {
    LOG_HB = 0,
    LOG_TPDO,
    LOG_RPDO,
    LOG_SDO_RX,
    LOG_SDO_TX,
    LOG_NMT,
    LOG_SYNC,
    LOG_EMCY,
    LOG_OTHER,
    LOG_CLASS_MAX
} LogClass;

void canlog_enable(uint8_t en);
void canlog_only_set(uint16_t cob);
void canlog_class_enable(LogClass cls, uint8_t en);
void canlog_blk_add(uint16_t cob);
void canlog_blk_del(uint16_t cob);
void canlog_status_print(void);
void canlog_on_rx_frame_print(const Message *m);

#endif
