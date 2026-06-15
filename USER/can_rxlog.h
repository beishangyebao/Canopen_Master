#ifndef CAN_RXLOG_H
#define CAN_RXLOG_H

#include <stdint.h>
#include "canfestival.h"
#include "can_log.h"

#define CAN_RXLOG_ENABLE 1
#define CAN_RXLOG_QSIZE 64

void can_rxlog_push_isr(const Message *m);
void can_rxlog_poll_print(void);
int can_rxlog_pop(Message *out);

#endif
