#ifndef __CONTROL_H
#define __CONTROL_H

#include "./SYSTEM/sys/sys.h"

/* Right-mode laser-1 judgement range after applying the per-step offset.
 * Values inside this inclusive range are reported as 350 mm.
 */
#define RIGHT_LASER_RANGE_MIN_MM              336
#define RIGHT_LASER_RANGE_MAX_MM              395
#define RIGHT_LASER_RANGE_CENTER_MM           365

/* Subtract 90 mm in protocol steps 2, 3, 6, and 7; subtract 40 mm in all
 * other protocol steps before applying the 300..400 mm range test.
 */
#define RIGHT_LASER_SPECIAL_STEP_OFFSET_MM    100
#define RIGHT_LASER_NORMAL_STEP_OFFSET_MM     50

#define RIGHT_PROTOCOL_CYCLE_COUNT            8U

typedef enum
{
    MOTION_STOP = 0,
    MOTION_FORWARD,
    MOTION_BACKWARD,
    MOTION_SPIN_CW,
    MOTION_SPIN_CCW,
    MOTION_SHIFT_LEFT,
    MOTION_SHIFT_RIGHT,
} MotionState;

extern volatile MotionState g_motion_state;
extern volatile uint8_t g_ultrasonic_stop_flag;
extern volatile uint8_t g_ultrasonic_parking_latched;
/* TIM3 is the front-wheel encoder; TIM4 is the rear-wheel encoder. */
extern volatile int32_t g_encoder_speed_front;
extern volatile int32_t g_encoder_speed_rear;

/* Right-mode laser protocol state: 1=requested, 2=result ready, 0=idle. */
extern volatile uint8_t g_laser_flag;
extern volatile int32_t g_laser_value;

void car_forward(void);
void car_forward_with_goal(int32_t goal_abs);
/* Bare 11: use MOTION_PWM_DEFAULT for the whole movement. */
void car_forward_with_goal_bare_11(int32_t goal_abs);
/* 11+xx: independent fixed PWM from control.c for the whole movement. */
void car_forward_with_goal_suffix_pwm(int32_t goal_abs);
void car_backward(void);
void car_backward_with_goal(int32_t goal_abs);
/* Bare 18: use MOTION_PWM_DEFAULT for the whole movement. */
void car_backward_with_goal_bare_18(int32_t goal_abs);
/* 18+xx: independent fixed PWM from control.c for the whole movement. */
void car_backward_with_goal_suffix_pwm(int32_t goal_abs);
void car_spin_clockwise(void);
void car_spin_counterclockwise(void);
void car_shift_left(void);
void car_shift_left_with_goal(int32_t goal_abs);
void car_shift_right(void);
void car_shift_right_with_goal(int32_t goal_abs);
void car_cancel_motion(void);
void car_ultrasonic_parking_reset(void);
/* 14+1 enters protocol state 3; 14+0 restores the saved state and movement. */
void car_protocol_command14(uint8_t parking);
/* Both modes: true during bare 11/18, 12/13, or their state-3 pause. */
uint8_t car_ultrasonic_monitor_enabled(void);
/* Called only from the fixed 10 ms TIM2 interrupt after encoder sampling. */
void car_encoder_control_10ms(void);
void car_motion_process(void);

/* Both modes use protocol state 1/state 2. During state 1, an unexpected
 * command switches to unrestricted state 2. State 3 blocks normal commands.
 */
uint8_t car_protocol_command_allowed(uint8_t cmd, uint8_t suffix_present);
/* For state-1 15+xx/16+xx: right mode always reports 31; left mode is silent.
 * Both modes then wait for the final 11/18/19 step. In right mode, if no final
 * command arrives within 5 seconds after 31, it sends 19 and then 22 locally.
 */
void car_protocol_report_lateral_by_step(void);
void car_protocol_command19(void);
void car_protocol_reset(void);
void car_laser_distance_judge_process(void);
void car_protocol_report_process(void);

/* Compatibility name kept for old calls. It maps to clockwise rotation. */
void car_spin_90d(void);

#endif
