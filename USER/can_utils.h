#ifndef CAN_UTILS_H
#define CAN_UTILS_H

#include <stdint.h>
#include "canfestival.h"

#define ODT_I8  0x02
#define ODT_I16 0x03
#define ODT_U8  0x05
#define ODT_U16 0x06
#define ODT_U32 0x07

void can_utils_put_u16_le(uint8_t *d, uint16_t v);
void can_utils_put_u32_le(uint8_t *d, uint32_t v);
int can_utils_send_ext(UNS32 cob_id, const uint8_t *data, uint8_t len);
int sendRPDO_conf(uint8_t node_id, uint8_t pdo_num, const uint8_t *data, uint8_t len);
void sendNMT(uint8_t node_id, uint8_t command);
int canopen_send_sdo(uint8_t node_id, uint16_t index, uint8_t subindex,
                     uint8_t data_type, uint32_t data, uint8_t data_size);
void canopen_read_sdo_print(uint8_t node_id, uint16_t index, uint8_t subindex);

#endif
