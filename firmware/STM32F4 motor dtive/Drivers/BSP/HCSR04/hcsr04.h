#ifndef __HCSR04_H
#define __HCSR04_H

#include "./SYSTEM/sys/sys.h"

/* HC-SR04 GPIO wiring inherited from the original 2_2_2 project. */
#define HCSR04_TRIG_GPIO_PORT          GPIOA
#define HCSR04_TRIG_GPIO_PIN           GPIO_PIN_2
#define HCSR04_ECHO_GPIO_PORT          GPIOA
#define HCSR04_ECHO_GPIO_PIN           GPIO_PIN_3

#define HCSR04_TIMEOUT_US              25000U
#define HCSR04_MEASURE_INTERVAL_MS     50U
#define HCSR04_NEAR_THRESHOLD_MM       250U
#define HCSR04_NEAR_CONFIRM_COUNT      3U

extern volatile uint16_t g_hcsr04_distance_mm;
extern volatile uint8_t g_hcsr04_measure_ok;
extern volatile uint8_t g_hcsr04_near_count;

void hcsr04_init(void);
void hcsr04_service(void);
void hcsr04_parking_reset(void);
uint8_t hcsr04_read_mm(uint16_t *distance_mm);

#endif
