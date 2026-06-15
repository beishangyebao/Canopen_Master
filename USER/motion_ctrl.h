#ifndef MOTION_CTRL_H
#define MOTION_CTRL_H

#include <stdint.h>

void motion_pv_run(uint8_t node, int32_t speed);
void motion_cv_set_speed(uint8_t node, int32_t speed);
void motion_pp_init_enable(uint8_t node);
void motion_pp_move_abs(uint8_t node, int32_t target_abs);
void motion_pp_move_rel(uint8_t node, int32_t distance);
void motion_homing_run(uint8_t node, int8_t method);
void motion_pt_run(uint8_t node, int16_t torque);
void motion_ct_set_torque(uint8_t node, int16_t torque);

#endif
