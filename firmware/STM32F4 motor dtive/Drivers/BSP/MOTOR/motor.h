#ifndef __MOTOR_H
#define __MOTOR_H

#include "./SYSTEM/sys/sys.h"

void motor_pwm_set(int16_t front, int16_t rear);
void motor_shift_pwm_set(int16_t front, int16_t rear);
void motor_pwm_output_10ms(void);

#endif
