#ifndef DS402_MASTER_H
#define DS402_MASTER_H

#include <stdint.h>

/*
 * DS402 主站侧 PDS 状态枚举。
 * 该枚举只描述 0x6041 Statusword 的状态机主状态，
 * bit4、bit5、bit9、bit10、bit12、bit13 等附加位由打印函数单独解析。
 */
typedef enum
{
    DS402_MASTER_STATE_UNKNOWN = 0,
    DS402_MASTER_STATE_NOT_READY_TO_SWITCH_ON,
    DS402_MASTER_STATE_SWITCH_ON_DISABLED,
    DS402_MASTER_STATE_READY_TO_SWITCH_ON,
    DS402_MASTER_STATE_SWITCHED_ON,
    DS402_MASTER_STATE_OPERATION_ENABLED,
    DS402_MASTER_STATE_QUICK_STOP_ACTIVE,
    DS402_MASTER_STATE_FAULT_REACTION_ACTIVE,
    DS402_MASTER_STATE_FAULT
} Ds402MasterState;

Ds402MasterState ds402_master_decode_status(uint16_t sw);
const char *ds402_master_state_name(Ds402MasterState state);
int ds402_master_wait_state(Ds402MasterState expected, uint32_t timeout_ms);
int ds402_master_enable_operation(uint8_t node);
int ds402_master_fault_reset(uint8_t node);
int ds402_master_disable_voltage(uint8_t node);
int ds402_master_shutdown(uint8_t node);
int ds402_master_quick_stop(uint8_t node);
int ds402_master_is_operation_enabled(void);
void ds402_master_print_status(uint16_t sw);

#endif
