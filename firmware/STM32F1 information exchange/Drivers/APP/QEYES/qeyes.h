#ifndef __QEYES_H
#define __QEYES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include "./BSP/HUB75/hub75.h"
#include <stdint.h>

void qeyes_init(void);
void qeyes_task(void);

void qeyes_show_closed(void);
void qeyes_show_open(void);
void qeyes_show_heart(void);
void qeyes_show_confused(void);
void qeyes_show_squint(void);
void qeyes_show_angry(void);
void qeyes_show_tear(void);

#ifdef __cplusplus
}
#endif

#endif
