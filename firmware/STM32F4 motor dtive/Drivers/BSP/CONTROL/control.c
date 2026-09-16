#include "./BSP/CONTROL/control.h"
#include "./BSP/MOTOR/motor.h"
#include "./BSP/MOTOR/motor_uart.h"
#include "./BSP/ATK_MS53L0/jiguang.h"
#include <stdio.h>

/* Motion parameters are owned by control.c.
 * All tick values use the fixed 10 ms TIM2 control period.
 */
#define MOTION_PWM_DEFAULT               6000
#define MOTION_GOAL_DEFAULT              MOTOR_GOAL_ABS
#define SUFFIX_LINEAR_PWM_DEFAULT        6000  /* 11+xx and 18+xx full-time PWM */
#define SPIN_PWM_DEFAULT                  6000  /* 12 and 13 full-time PWM */
#define ULTRASONIC_ACTION_DELAY_TICKS_10MS 50U  /* local sender: 500 ms after sending 14+x */
#define ULTRASONIC_RESTART_LOCK_TICKS_10MS 100U /* after resume, ignore local ultrasonic stop for 1 s */
#define ULTRASONIC_STOP_LOCK_TICKS_10MS    100U /* after parking, ignore local ultrasonic resume for 1 s */
#define RIGHT_FINAL_WAIT_TIMEOUT_MS          5000U /* right mode: after 31, auto-finish if no final command */

#define SHIFT_PWM_DEFAULT                6000
/* Add a small forward component while shifting left to cancel backward drift.
 * Increase this value if it still drifts backward; decrease it if it drifts forward.
 */
#define SHIFT_LEFT_FORWARD_COMPENSATION_PWM 150
#define LASER_VALUE_DEFAULT     RIGHT_LASER_RANGE_CENTER_MM

typedef enum
{
    PROTOCOL_FLOW_WAIT_PRIMARY = 0,
    PROTOCOL_FLOW_LASER_JUDGE,
    PROTOCOL_FLOW_WAIT_LATERAL,
    PROTOCOL_FLOW_WAIT_FINAL,
} ProtocolFlowState;

typedef enum
{
    PROTOCOL_STATE1_FLOW = 1,
    PROTOCOL_STATE2_FREE,
    PROTOCOL_STATE3_PARKED,
} ProtocolMode;

typedef enum
{
    LINEAR_PWM_USE_DEFAULT = 0,
    LINEAR_PWM_SUFFIX_FIXED,
} LinearPwmMode;

volatile MotionState g_motion_state = MOTION_STOP;
volatile uint8_t g_ultrasonic_stop_flag = 0U;
/* Set while protocol state 3 is holding a paused movement. */
volatile uint8_t g_ultrasonic_parking_latched = 0U;
volatile int32_t g_encoder_speed_front = 0;
volatile int32_t g_encoder_speed_rear = 0;

volatile uint8_t g_laser_flag = 0U;
volatile int32_t g_laser_value = LASER_VALUE_DEFAULT;

/* Encoder-based position accumulator. Updated in the TIM2 10 ms ISR. */
static volatile int32_t current = 0;
static volatile int32_t s_motion_goal = MOTION_GOAL_DEFAULT;
/* Bare commands use the default PWM; suffixed commands use fixed PWM. */
static volatile LinearPwmMode s_linear_pwm_mode = LINEAR_PWM_USE_DEFAULT;
static int8_t s_linear_dir = 1;
static volatile uint8_t s_laser_measurement_started = 0U;
static volatile uint8_t s_protocol_completed_cycles = 0U;
static volatile ProtocolFlowState s_protocol_flow_state = PROTOCOL_FLOW_WAIT_PRIMARY;
static volatile ProtocolMode s_protocol_mode = PROTOCOL_STATE1_FLOW;
static volatile ProtocolMode s_protocol_resume_mode = PROTOCOL_STATE1_FLOW;
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
/* Starts immediately after sending 31. A valid final 11/18/19 command cancels it. */
static volatile uint8_t s_right_final_wait_timeout_active = 0U;
static volatile uint8_t s_right_final_wait_timeout_finalizing = 0U;
static volatile uint32_t s_right_final_wait_start_tick = 0U;
#endif
/* State-3 pause snapshot. Commands 11/18/12/13 all resume from the saved encoder progress. */
static volatile MotionState s_parked_motion_state = MOTION_STOP;
static volatile int32_t s_parked_encoder_current = 0;
/* Protocol command context used to gate ultrasonic monitoring in both modes. */
static volatile uint8_t s_active_motion_cmd = 0U;
static volatile uint8_t s_active_motion_suffix_present = 0U;
/* Encoder-based motion completion is latched in TIM2 and finalized in the main loop. */
static volatile MotionState s_encoder_completed_pending = MOTION_STOP;

typedef enum
{
    ULTRASONIC_DELAY_NONE = 0,
    ULTRASONIC_DELAY_STOP,
    ULTRASONIC_DELAY_RESUME,
} UltrasonicDelayAction;

static volatile UltrasonicDelayAction s_ultrasonic_delay_action = ULTRASONIC_DELAY_NONE;
static volatile uint16_t s_ultrasonic_delay_ticks = 0U;
static volatile uint8_t s_ultrasonic_delay_due = 0U;
/* Set only on the side that detected the obstacle and transmitted 14+1. */
static volatile uint8_t s_ultrasonic_local_owner = 0U;
/* Prevent immediate repeated parking after either side resumes from state 3. */
static volatile uint16_t s_ultrasonic_restart_lock_ticks = 0U;
/* Prevent an immediate local resume decision after entering state 3. */
static volatile uint16_t s_ultrasonic_stop_lock_ticks = 0U;

static const uint8_t s_right_primary_sequence[RIGHT_PROTOCOL_CYCLE_COUNT + 1U] =
{
    '1', '2', '1', '3', '1', '3', '1', '2', '1'
};

#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
static void car_right_final_wait_timeout_cancel(void)
{
    s_right_final_wait_timeout_active = 0U;
    s_right_final_wait_start_tick = 0U;
}

static void car_right_final_wait_timeout_start(void)
{
    s_right_final_wait_start_tick = HAL_GetTick();
    s_right_final_wait_timeout_active = 1U;
}

/* Atomically claims an expired timeout so a final UART command cannot race the
 * main loop and cause both the received-command path and timeout path to run.
 */
static uint8_t car_right_final_wait_timeout_take_due(void)
{
    uint8_t due = 0U;
    uint32_t now = HAL_GetTick();
    uint32_t primask;

    if ((s_right_final_wait_timeout_active == 0U) ||
        (s_protocol_mode != PROTOCOL_STATE1_FLOW) ||
        (s_protocol_flow_state != PROTOCOL_FLOW_WAIT_FINAL) ||
        ((uint32_t)(now - s_right_final_wait_start_tick) < RIGHT_FINAL_WAIT_TIMEOUT_MS))
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    now = HAL_GetTick();
    if ((s_right_final_wait_timeout_active != 0U) &&
        (s_protocol_mode == PROTOCOL_STATE1_FLOW) &&
        (s_protocol_flow_state == PROTOCOL_FLOW_WAIT_FINAL) &&
        ((uint32_t)(now - s_right_final_wait_start_tick) >= RIGHT_FINAL_WAIT_TIMEOUT_MS))
    {
        car_right_final_wait_timeout_cancel();
        s_right_final_wait_timeout_finalizing = 1U;
        due = 1U;
    }
    if (primask == 0U)
    {
        __enable_irq();
    }

    return due;
}
#endif

static void car_protocol_finish_cycle(void)
{
    if (s_protocol_completed_cycles < RIGHT_PROTOCOL_CYCLE_COUNT)
    {
        s_protocol_completed_cycles++;
    }
    s_protocol_flow_state = PROTOCOL_FLOW_WAIT_PRIMARY;
}

#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
static uint8_t car_right_current_cycle_uses_laser_offset(void)
{
    uint8_t current_cycle = (uint8_t)(s_protocol_completed_cycles + 1U);

    return ((current_cycle == 2U) || (current_cycle == 3U) ||
            (current_cycle == 6U) || (current_cycle == 7U)) ? 1U : 0U;
}
#endif

static void car_protocol_clear_flow_runtime(void)
{
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
    jiguang_cancel_query();
    car_right_final_wait_timeout_cancel();
    s_right_final_wait_timeout_finalizing = 0U;
#endif
    g_laser_flag = 0U;
    g_laser_value = LASER_VALUE_DEFAULT;
    s_laser_measurement_started = 0U;
    s_protocol_completed_cycles = 0U;
    s_protocol_flow_state = PROTOCOL_FLOW_WAIT_PRIMARY;
}

static void car_protocol_enter_free_mode(void)
{
    if (s_protocol_mode == PROTOCOL_STATE3_PARKED)
    {
        s_protocol_resume_mode = PROTOCOL_STATE2_FREE;
    }
    else
    {
        s_protocol_mode = PROTOCOL_STATE2_FREE;
    }
    car_protocol_clear_flow_runtime();
}

static ProtocolMode car_protocol_effective_mode(void)
{
    return (s_protocol_mode == PROTOCOL_STATE3_PARKED) ?
           s_protocol_resume_mode : s_protocol_mode;
}

static uint8_t car_protocol_command_is_expected(uint8_t cmd, uint8_t suffix_present)
{
    uint8_t motion_busy;

    motion_busy = (g_motion_state != MOTION_STOP) ? 1U : 0U;

    if (motion_busy != 0U)
    {
        return 0U;
    }

    switch (s_protocol_flow_state)
    {
        case PROTOCOL_FLOW_WAIT_PRIMARY:
            if (s_protocol_completed_cycles <= RIGHT_PROTOCOL_CYCLE_COUNT)
            {
                return (cmd == s_right_primary_sequence[s_protocol_completed_cycles]) ? 1U : 0U;
            }
            return 0U;

        case PROTOCOL_FLOW_WAIT_LATERAL:
            return (((cmd == '5') || (cmd == '6')) &&
                    (suffix_present != 0U)) ? 1U : 0U;

        case PROTOCOL_FLOW_WAIT_FINAL:
            return ((cmd == '1') || (cmd == '8') || (cmd == '9')) ? 1U : 0U;

        case PROTOCOL_FLOW_LASER_JUDGE:
        default:
            return 0U;
    }
}

static int16_t dir_polarity(int16_t dir)
{
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
    return (int16_t)(-dir);
#else
    return dir;
#endif
}

static int16_t control_limit_pwm(int32_t pwm)
{
    if (pwm > MOTOR_PWM_MAX_DUTY)
    {
        pwm = MOTOR_PWM_MAX_DUTY;
    }
    else if (pwm < -MOTOR_PWM_MAX_DUTY)
    {
        pwm = -MOTOR_PWM_MAX_DUTY;
    }

    return (int16_t)pwm;
}

static int32_t control_abs32(int32_t v)
{
    return (v < 0) ? -v : v;
}

static int32_t control_encoder_average_abs(void)
{
    return (control_abs32(g_encoder_speed_front) +
            control_abs32(g_encoder_speed_rear)) / 2;
}

static int32_t control_make_goal_abs(int32_t goal)
{
    goal = control_abs32(goal);
    if (goal < 1)
    {
        goal = MOTOR_GOAL_ABS;
    }

    return goal;
}


static int32_t control_motion_pwm_abs(void)
{
    return MOTION_PWM_DEFAULT;
}

static uint8_t car_active_motion_ultrasonic_eligible(void)
{
    if ((s_active_motion_cmd == '1') &&
        (s_active_motion_suffix_present == 0U) &&
        (g_motion_state == MOTION_FORWARD))
    {
        return 1U;
    }

    if ((s_active_motion_cmd == '8') &&
        (s_active_motion_suffix_present == 0U) &&
        (g_motion_state == MOTION_BACKWARD))
    {
        return 1U;
    }

    if (((s_active_motion_cmd == '2') &&
         (g_motion_state == MOTION_SPIN_CW)) ||
        ((s_active_motion_cmd == '3') &&
         (g_motion_state == MOTION_SPIN_CCW)))
    {
        return 1U;
    }

    return 0U;
}

uint8_t car_ultrasonic_monitor_enabled(void)
{
    /* During the one-second restart lock, discard local ultrasonic samples.
     * A peer 14+1 command is still handled immediately by command processing.
     */
    if (s_ultrasonic_restart_lock_ticks > 0U)
    {
        return 0U;
    }

    if ((g_ultrasonic_parking_latched != 0U) &&
        (s_ultrasonic_local_owner != 0U))
    {
        return 1U;
    }

    return car_active_motion_ultrasonic_eligible();
}

static void car_ultrasonic_delay_reset(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    s_ultrasonic_delay_action = ULTRASONIC_DELAY_NONE;
    s_ultrasonic_delay_ticks = 0U;
    s_ultrasonic_delay_due = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

static void car_ultrasonic_delay_schedule(UltrasonicDelayAction action)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    s_ultrasonic_delay_action = action;
    s_ultrasonic_delay_ticks = ULTRASONIC_ACTION_DELAY_TICKS_10MS;
    s_ultrasonic_delay_due = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

static void car_ultrasonic_delay_tick_10ms(void)
{
    if (s_ultrasonic_restart_lock_ticks > 0U)
    {
        s_ultrasonic_restart_lock_ticks--;
    }

    if (s_ultrasonic_stop_lock_ticks > 0U)
    {
        s_ultrasonic_stop_lock_ticks--;
    }

    if ((s_ultrasonic_delay_action != ULTRASONIC_DELAY_NONE) &&
        (s_ultrasonic_delay_ticks > 0U))
    {
        s_ultrasonic_delay_ticks--;
        if (s_ultrasonic_delay_ticks == 0U)
        {
            s_ultrasonic_delay_due = 1U;
        }
    }
}

static void car_clear_parking_snapshot(void)
{
    s_parked_motion_state = MOTION_STOP;
    s_parked_encoder_current = 0;
}

static void car_enter_parking_state(void)
{
    uint32_t primask;

    if (s_protocol_mode == PROTOCOL_STATE3_PARKED)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    s_protocol_resume_mode = s_protocol_mode;
    s_parked_motion_state = g_motion_state;
    s_parked_encoder_current = current;
    motor_pwm_set(0, 0);
    g_motion_state = MOTION_STOP;
    s_protocol_mode = PROTOCOL_STATE3_PARKED;
    /* Keep sampling, but do not decide to resume during the first second. */
    s_ultrasonic_stop_lock_ticks = ULTRASONIC_STOP_LOCK_TICKS_10MS;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

static void car_resume_from_parking_state(void)
{
    uint32_t primask;

    if (s_protocol_mode != PROTOCOL_STATE3_PARKED)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    current = s_parked_encoder_current;
    g_motion_state = s_parked_motion_state;
    s_protocol_mode = s_protocol_resume_mode;
    /* Restart motion first, then lock out local stop recognition for at least
     * 100 fixed 10 ms control periods. Samples restart cleanly after expiry.
     */
    s_ultrasonic_stop_lock_ticks = 0U;
    s_ultrasonic_restart_lock_ticks = ULTRASONIC_RESTART_LOCK_TICKS_10MS;
    car_clear_parking_snapshot();
    if (primask == 0U)
    {
        __enable_irq();
    }
}

static void car_ultrasonic_delay_process(void)
{
    UltrasonicDelayAction action;
    uint32_t primask;

    if (s_ultrasonic_delay_due == 0U)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    action = s_ultrasonic_delay_action;
    s_ultrasonic_delay_action = ULTRASONIC_DELAY_NONE;
    s_ultrasonic_delay_ticks = 0U;
    s_ultrasonic_delay_due = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }

    if (action == ULTRASONIC_DELAY_STOP)
    {
        car_enter_parking_state();
    }
    else if (action == ULTRASONIC_DELAY_RESUME)
    {
        car_resume_from_parking_state();
        g_ultrasonic_parking_latched = 0U;
        s_ultrasonic_local_owner = 0U;
    }
}

static void car_stop_internal(void)
{
    uint32_t primask = __get_PRIMASK();

    /* Keep motion state and PWM target coherent with the TIM2 control ISR. */
    __disable_irq();
    motor_pwm_set(0, 0);
    g_motion_state = MOTION_STOP;
    current = 0;
    s_motion_goal = MOTION_GOAL_DEFAULT;
    s_linear_pwm_mode = LINEAR_PWM_USE_DEFAULT;
    s_linear_dir = 1;
    s_active_motion_cmd = 0U;
    s_active_motion_suffix_present = 0U;
    s_encoder_completed_pending = MOTION_STOP;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

/*
 * Completion actions may stop a laser measurement or print protocol data, so
 * they must run in the main loop rather than in the TIM2 interrupt.
 */
static void car_complete_motion_action(MotionState completed_state)
{
    if (car_protocol_effective_mode() != PROTOCOL_STATE1_FLOW)
    {
        return;
    }

    /* After eight complete cycles, the final expected 11 executes normally.
     * When that movement stops, enter unrestricted state 2 without 21+xx.
     */
    if ((s_protocol_flow_state == PROTOCOL_FLOW_WAIT_PRIMARY) &&
        (s_protocol_completed_cycles >= RIGHT_PROTOCOL_CYCLE_COUNT) &&
        (completed_state == MOTION_FORWARD))
    {
        car_protocol_enter_free_mode();
        return;
    }

    if ((s_protocol_flow_state == PROTOCOL_FLOW_WAIT_PRIMARY) &&
        ((completed_state == MOTION_FORWARD) ||
         (completed_state == MOTION_SPIN_CW) ||
         (completed_state == MOTION_SPIN_CCW)))
    {
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
        g_laser_value = LASER_VALUE_DEFAULT;
        s_laser_measurement_started = 0U;
        g_laser_flag = 1U;
        s_protocol_flow_state = PROTOCOL_FLOW_LASER_JUDGE;
#else
        /* Left mode tracks the same state-1 sequence but sends no 21+xx. */
        s_protocol_flow_state = PROTOCOL_FLOW_WAIT_LATERAL;
#endif
        return;
    }

    if ((s_protocol_flow_state == PROTOCOL_FLOW_WAIT_FINAL) &&
        ((completed_state == MOTION_FORWARD) ||
         (completed_state == MOTION_BACKWARD)))
    {
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
        printf("22\r\n");
#endif
        car_protocol_finish_cycle();
    }
}

/* TIM2 ISR only: stop PWM immediately and defer non-real-time completion work. */
static void car_complete_encoder_motion_10ms(MotionState completed_state)
{
    car_stop_internal();
    s_encoder_completed_pending = completed_state;
}

static void car_encoder_completion_process(void)
{
    MotionState completed_state;
    uint32_t primask;

    primask = __get_PRIMASK();
    __disable_irq();
    completed_state = s_encoder_completed_pending;
    s_encoder_completed_pending = MOTION_STOP;
    if (primask == 0U)
    {
        __enable_irq();
    }

    if (completed_state != MOTION_STOP)
    {
        car_complete_motion_action(completed_state);
    }
}

static void car_start_linear_motion(int8_t linear_dir, int32_t goal_abs,
                                    LinearPwmMode pwm_mode,
                                    uint8_t protocol_cmd,
                                    uint8_t suffix_present)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    current = 0;
    s_linear_dir = (linear_dir < 0) ? -1 : 1;
    s_linear_pwm_mode = pwm_mode;
    s_motion_goal = control_make_goal_abs(goal_abs);
    s_active_motion_cmd = protocol_cmd;
    s_active_motion_suffix_present = suffix_present;
    s_encoder_completed_pending = MOTION_STOP;
    g_motion_state = (s_linear_dir < 0) ? MOTION_BACKWARD : MOTION_FORWARD;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void car_forward(void)
{
    car_start_linear_motion(1, 0, LINEAR_PWM_USE_DEFAULT, 0U, 0U);
}

void car_forward_with_goal(int32_t goal_abs)
{
    car_start_linear_motion(1, goal_abs, LINEAR_PWM_USE_DEFAULT, 0U, 0U);
}

void car_forward_with_goal_bare_11(int32_t goal_abs)
{
    car_start_linear_motion(1, goal_abs, LINEAR_PWM_USE_DEFAULT, '1', 0U);
}

void car_forward_with_goal_suffix_pwm(int32_t goal_abs)
{
    car_start_linear_motion(1, goal_abs, LINEAR_PWM_SUFFIX_FIXED, '1', 1U);
}

void car_backward(void)
{
    car_start_linear_motion(-1, 0, LINEAR_PWM_USE_DEFAULT, 0U, 0U);
}

void car_backward_with_goal(int32_t goal_abs)
{
    car_start_linear_motion(-1, goal_abs, LINEAR_PWM_USE_DEFAULT, 0U, 0U);
}

void car_backward_with_goal_bare_18(int32_t goal_abs)
{
    car_start_linear_motion(-1, goal_abs, LINEAR_PWM_USE_DEFAULT, '8', 0U);
}

void car_backward_with_goal_suffix_pwm(int32_t goal_abs)
{
    car_start_linear_motion(-1, goal_abs, LINEAR_PWM_SUFFIX_FIXED, '8', 1U);
}

void car_spin_clockwise(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    current = 0;
    s_motion_goal = control_abs32(MOTOR_SPIN_GOAL_ABS);
    s_active_motion_cmd = '2';
    s_active_motion_suffix_present = 0U;
    s_encoder_completed_pending = MOTION_STOP;
    g_motion_state = MOTION_SPIN_CW;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void car_spin_counterclockwise(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    current = 0;
    s_motion_goal = control_abs32(MOTOR_SPIN_GOAL_ABS);
    s_active_motion_cmd = '3';
    s_active_motion_suffix_present = 0U;
    s_encoder_completed_pending = MOTION_STOP;
    g_motion_state = MOTION_SPIN_CCW;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void car_spin_90d(void)
{
    car_spin_clockwise();
}

static void car_start_shift_motion(int8_t left_dir, int32_t goal_abs,
                                   uint8_t protocol_cmd)
{
    uint32_t primask;

    goal_abs = control_make_goal_abs(goal_abs);

    primask = __get_PRIMASK();
    __disable_irq();
    current = 0;
    s_motion_goal = goal_abs;
    s_active_motion_cmd = protocol_cmd;
    s_active_motion_suffix_present = 1U;
    s_encoder_completed_pending = MOTION_STOP;
    g_motion_state = (left_dir > 0) ? MOTION_SHIFT_LEFT : MOTION_SHIFT_RIGHT;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void car_shift_left(void)
{
    car_start_shift_motion(1, 0, '5');
}

void car_shift_left_with_goal(int32_t goal_abs)
{
    car_start_shift_motion(1, goal_abs, '5');
}

void car_shift_right(void)
{
    car_start_shift_motion(-1, 0, '6');
}

void car_shift_right_with_goal(int32_t goal_abs)
{
    car_start_shift_motion(-1, goal_abs, '6');
}

void car_cancel_motion(void)
{
    if (s_protocol_mode == PROTOCOL_STATE3_PARKED)
    {
        s_protocol_mode = s_protocol_resume_mode;
    }
    car_stop_internal();
    car_clear_parking_snapshot();
    car_ultrasonic_delay_reset();
    g_ultrasonic_parking_latched = 0U;
    s_ultrasonic_local_owner = 0U;
    s_ultrasonic_restart_lock_ticks = 0U;
    s_ultrasonic_stop_lock_ticks = 0U;
}

uint8_t car_protocol_command_allowed(uint8_t cmd, uint8_t suffix_present)
{
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
    /* Once the 5-second timeout is claimed, late final commands must not race
     * the outgoing 19/22 pair and complete the same step a second time.
     */
    if (s_right_final_wait_timeout_finalizing != 0U)
    {
        return 0U;
    }
#endif

    if (s_protocol_mode == PROTOCOL_STATE3_PARKED)
    {
        return 0U;
    }

    if (s_protocol_mode == PROTOCOL_STATE2_FREE)
    {
        return 1U;
    }

    if (car_protocol_command_is_expected(cmd, suffix_present) != 0U)
    {
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
        /* Any accepted final command means the peer answered before 5 seconds.
         * This includes the normal suffixed 11/18 forms and command 19.
         */
        if ((s_protocol_flow_state == PROTOCOL_FLOW_WAIT_FINAL) &&
            ((cmd == '1') || (cmd == '8') || (cmd == '9')))
        {
            car_right_final_wait_timeout_cancel();
        }
#endif
        return 1U;
    }

    /* Both left and right modes use state 1/state 2. An out-of-sequence
     * command exits the coordinated flow and executes in free state 2.
     */
    car_protocol_enter_free_mode();
    return 1U;
}

void car_protocol_report_lateral_by_step(void)
{
    if ((s_protocol_mode == PROTOCOL_STATE1_FLOW) &&
        (s_protocol_flow_state == PROTOCOL_FLOW_WAIT_LATERAL))
    {
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
        /* Every right-mode protocol step reports 31; 38 is retired. */
        printf("31\r\n");
#endif
        /* Left mode performs the same transition without sending anything. */
        s_protocol_flow_state = PROTOCOL_FLOW_WAIT_FINAL;
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
        /* Start only after 31 has been transmitted and WAIT_FINAL is active. */
        car_right_final_wait_timeout_start();
#endif
    }
}

void car_protocol_command19(void)
{
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
    car_right_final_wait_timeout_cancel();
#endif
    car_cancel_motion();

    if ((s_protocol_mode == PROTOCOL_STATE1_FLOW) &&
        (s_protocol_flow_state == PROTOCOL_FLOW_WAIT_FINAL))
    {
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
        printf("22\r\n");
#endif
        car_protocol_finish_cycle();
    }
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
    s_right_final_wait_timeout_finalizing = 0U;
#endif
}

void car_protocol_reset(void)
{
    car_cancel_motion();
    s_protocol_mode = PROTOCOL_STATE1_FLOW;
    s_protocol_resume_mode = PROTOCOL_STATE1_FLOW;
    car_clear_parking_snapshot();
    car_protocol_clear_flow_runtime();
}

void car_laser_distance_judge_process(void)
{
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
    uint16_t average;
    int32_t judged_value;

    if ((s_protocol_mode == PROTOCOL_STATE1_FLOW) &&
        (s_protocol_flow_state == PROTOCOL_FLOW_LASER_JUDGE) &&
        (g_laser_flag == 1U))
    {
        if (s_laser_measurement_started == 0U)
        {
            if (jiguang_request_average_silent() != 0U)
            {
                g_laser_value = 0;
                g_laser_flag = 2U;
                return;
            }
            s_laser_measurement_started = 1U;
        }

        if (jiguang_take_average_result(&average) != 0U)
        {
            judged_value = (int32_t)average;

            if (car_right_current_cycle_uses_laser_offset() != 0U)
            {
                judged_value -= RIGHT_LASER_SPECIAL_STEP_OFFSET_MM;
            }
            else
            {
                judged_value -= RIGHT_LASER_NORMAL_STEP_OFFSET_MM;
            }

            if (judged_value < 0)
            {
                judged_value = 0;
            }

            if ((judged_value >= RIGHT_LASER_RANGE_MIN_MM) &&
                (judged_value <= RIGHT_LASER_RANGE_MAX_MM))
            {
                judged_value = RIGHT_LASER_RANGE_CENTER_MM;
            }

            g_laser_value = judged_value;
            s_laser_measurement_started = 0U;
            g_laser_flag = 2U;
        }
    }
#endif
}

void car_protocol_report_process(void)
{
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
    if (car_right_final_wait_timeout_take_due() != 0U)
    {
        /* Mirror a normal peer command-19 exchange: first tell the left side 19,
         * then complete the local WAIT_FINAL path, which sends 22 and advances
         * this side to the next primary-command wait state.
         */
        printf("19\r\n");
        car_protocol_command19();
    }

    if ((s_protocol_mode == PROTOCOL_STATE1_FLOW) &&
        (s_protocol_flow_state == PROTOCOL_FLOW_LASER_JUDGE) &&
        (g_laser_flag == 2U))
    {
        printf("21+%ld\r\n", (long)g_laser_value);
        g_laser_flag = 0U;
        s_protocol_flow_state = PROTOCOL_FLOW_WAIT_LATERAL;
    }

#endif
}

void car_protocol_command14(uint8_t parking)
{
    /* This function handles a command received from the peer. The receiver
     * parks/resumes immediately and must not run the sender-side sensor logic.
     */
    car_ultrasonic_delay_reset();
    s_ultrasonic_local_owner = 0U;

    if (parking != 0U)
    {
        g_ultrasonic_parking_latched = 1U;
        car_enter_parking_state();
    }
    else
    {
        car_resume_from_parking_state();
        g_ultrasonic_parking_latched = 0U;
    }
}

void car_ultrasonic_parking_reset(void)
{
    car_cancel_motion();
    g_ultrasonic_stop_flag = 0U;
    g_ultrasonic_parking_latched = 0U;
    s_ultrasonic_local_owner = 0U;
    s_ultrasonic_restart_lock_ticks = 0U;
    s_ultrasonic_stop_lock_ticks = 0U;
    car_clear_parking_snapshot();
}


static void car_goal_position_control_10ms(void)
{
    int32_t goal;
    int32_t pwm_abs;
    int16_t pwm_cmd;
    MotionState completed_state;
    uint8_t reached;

    if ((g_motion_state != MOTION_FORWARD) && (g_motion_state != MOTION_BACKWARD))
    {
        return;
    }

    completed_state = g_motion_state;
    current += control_encoder_average_abs();
    goal = control_abs32(s_motion_goal);

    /* Stop as soon as the accumulated encoder distance reaches or exceeds
     * the configured target distance. Both sides are compared by absolute value.
     */
    reached = (control_abs32(current) >= goal) ? 1U : 0U;

    if (reached != 0U)
    {
        car_complete_encoder_motion_10ms(completed_state);
        return;
    }

    s_linear_dir = (g_motion_state == MOTION_BACKWARD) ? -1 : 1;
    pwm_abs = control_motion_pwm_abs();

    /* Bare 11/18 use MOTION_PWM_DEFAULT for the whole movement.
     * 11+xx/18+xx use their independent fixed PWM for the whole movement.
     * Encoder accumulation and goal judgement remain active in every period.
     */
    if (s_linear_pwm_mode == LINEAR_PWM_SUFFIX_FIXED)
    {
        pwm_abs = SUFFIX_LINEAR_PWM_DEFAULT;
    }

    pwm_cmd = control_limit_pwm((int32_t)s_linear_dir * pwm_abs);
    motor_pwm_set(pwm_cmd, pwm_cmd);
}

static void car_spin_control_10ms(void)
{
    int32_t goal;
    int16_t pwm_cmd;
    int8_t logical_dir;
    MotionState completed_state = g_motion_state;

    if ((g_motion_state != MOTION_SPIN_CW) &&
        (g_motion_state != MOTION_SPIN_CCW))
    {
        return;
    }

    current += control_encoder_average_abs();
    goal = control_abs32(s_motion_goal);

    if (control_abs32(current) >= goal)
    {
        car_complete_encoder_motion_10ms(completed_state);
        return;
    }

    logical_dir = (g_motion_state == MOTION_SPIN_CCW) ? -1 : 1;
    pwm_cmd = control_limit_pwm((int32_t)dir_polarity(logical_dir) *
                                SPIN_PWM_DEFAULT);
    motor_pwm_set(pwm_cmd, pwm_cmd);
}

static void car_shift_control_10ms(int8_t left_dir)
{
    int32_t goal;
    int32_t pwm_abs = SHIFT_PWM_DEFAULT;
    int16_t shift_pwm;
    int16_t front_pwm;
    int16_t rear_pwm;
    int16_t forward_compensation;
    MotionState completed_state = g_motion_state;

    if ((g_motion_state != MOTION_SHIFT_RIGHT) &&
        (g_motion_state != MOTION_SHIFT_LEFT))
    {
        return;
    }

    current += control_encoder_average_abs();
    goal = control_abs32(s_motion_goal);

    if (control_abs32(current) >= goal)
    {
        car_complete_encoder_motion_10ms(completed_state);
        return;
    }

    shift_pwm = control_limit_pwm(pwm_abs);

    if (left_dir > 0)
    {
        /* Command 15: shift left.
         * Vehicle test showed the previous sign pair produced a right shift
         * (front-left and rear-right forward, the other diagonal reverse),
         * so both lateral PWM signs are inverted here.
         */
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
        front_pwm = (int16_t)(-shift_pwm);
        rear_pwm = shift_pwm;
#else
        front_pwm = shift_pwm;
        rear_pwm = (int16_t)(-shift_pwm);
#endif
        /* Add the same physical forward component to the front and rear motors.
         * dir_polarity() keeps the compensation direction correct in both builds.
         */
        forward_compensation = dir_polarity(SHIFT_LEFT_FORWARD_COMPENSATION_PWM);
        front_pwm = control_limit_pwm((int32_t)front_pwm + forward_compensation);
        rear_pwm = control_limit_pwm((int32_t)rear_pwm + forward_compensation);
        motor_shift_pwm_set(front_pwm, rear_pwm);
    }
    else
    {
        /* Command 16: shift right, exactly opposite to command 15. */
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
        motor_shift_pwm_set(shift_pwm, (int16_t)(-shift_pwm));
#else
        motor_shift_pwm_set((int16_t)(-shift_pwm), shift_pwm);
#endif
    }
}

void car_encoder_control_10ms(void)
{
    car_ultrasonic_delay_tick_10ms();

    switch (g_motion_state)
    {
        case MOTION_FORWARD:
        case MOTION_BACKWARD:
            car_goal_position_control_10ms();
            break;

        case MOTION_SPIN_CW:
        case MOTION_SPIN_CCW:
            car_spin_control_10ms();
            break;

        case MOTION_SHIFT_LEFT:
            car_shift_control_10ms(1);
            break;

        case MOTION_SHIFT_RIGHT:
            car_shift_control_10ms(-1);
            break;

        default:
            break;
    }
}

void car_motion_process(void)
{
    /* Encoder completion can race with an incoming 14+1. Finalize it first;
     * completion actions use the saved underlying mode while state 3 is active.
     */
    car_encoder_completion_process();
    car_ultrasonic_delay_process();

    /* Either hardware mode may be the ultrasonic sender. Send 14+1 first,
     * then wait 50 TIM2 periods before stopping locally. The peer stops as
     * soon as it receives 14+1.
     */
    if ((s_protocol_mode != PROTOCOL_STATE3_PARKED) &&
        (g_ultrasonic_parking_latched == 0U) &&
        (s_ultrasonic_delay_action == ULTRASONIC_DELAY_NONE) &&
        (g_ultrasonic_stop_flag != 0U) &&
        (car_active_motion_ultrasonic_eligible() != 0U))
    {
        printf("14+1\r\n");
        s_ultrasonic_local_owner = 1U;
        g_ultrasonic_parking_latched = 1U;
        car_ultrasonic_delay_schedule(ULTRASONIC_DELAY_STOP);
        return;
    }

    /* Once the three-consecutive-near condition is broken, send 14+0 first
     * and keep the local side parked for another 50 TIM2 periods. The peer
     * resumes immediately when it receives 14+0.
     */
    if ((s_protocol_mode == PROTOCOL_STATE3_PARKED) &&
        (g_ultrasonic_parking_latched != 0U) &&
        (s_ultrasonic_local_owner != 0U) &&
        (s_ultrasonic_stop_lock_ticks == 0U) &&
        (s_ultrasonic_delay_action == ULTRASONIC_DELAY_NONE) &&
        (g_ultrasonic_stop_flag == 0U))
    {
        printf("14+0\r\n");
        car_ultrasonic_delay_schedule(ULTRASONIC_DELAY_RESUME);
    }

    if (s_protocol_mode == PROTOCOL_STATE3_PARKED)
    {
        motor_pwm_set(0, 0);
        return;
    }

    switch (g_motion_state)
    {
        case MOTION_FORWARD:
        case MOTION_BACKWARD:
        case MOTION_SPIN_CW:
        case MOTION_SPIN_CCW:
        case MOTION_SHIFT_LEFT:
        case MOTION_SHIFT_RIGHT:
            /* Encoder accumulation, target judgement and PWM command update run in TIM2. */
            break;

        default:
            motor_pwm_set(0, 0);
            break;
    }
}
