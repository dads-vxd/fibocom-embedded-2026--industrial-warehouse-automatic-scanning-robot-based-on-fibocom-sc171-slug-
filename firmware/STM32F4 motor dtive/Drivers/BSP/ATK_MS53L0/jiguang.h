#ifndef __JIGUANG_H
#define __JIGUANG_H

#include "stdint.h"

#define JIGUANG_AVERAGE_SAMPLE_COUNT    10U
#define JIGUANG_SAMPLE_INTERVAL_MS      50U

/* Initialize the single opposite-facing ATK-MS53L0 / VL53L0X module. */
uint8_t jiguang_init(void);

uint16_t jiguang_average_10(const uint16_t samples[JIGUANG_AVERAGE_SAMPLE_COUNT]);

/* Command 171: take 10 samples and print one average. */
uint8_t jiguang_request_average(void);

/* Silent request used by the coordinated vehicle protocol. */
uint8_t jiguang_request_average_silent(void);
uint8_t jiguang_request_threshold_count(uint16_t threshold);

uint8_t jiguang_take_average_result(uint16_t *average);
uint8_t jiguang_take_threshold_result(uint8_t *greater_count,
                                      uint8_t *less_count,
                                      uint8_t *equal_count);

void jiguang_cancel_query(void);
void jiguang_query_process(void);

#endif
