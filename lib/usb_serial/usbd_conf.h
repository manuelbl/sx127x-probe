/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * USB Serial Configuration
 */

#ifndef USBD_CONF_H
#define USBD_CONF_H

#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
 extern "C" {
#endif


#define USBD_MAX_NUM_INTERFACES      1
#define USBD_MAX_NUM_CONFIGURATION   1
#define USBD_MAX_STR_DESC_SIZ        512
#define MAX_STATIC_ALLOC_SIZE        512
#define DEVICE_FS                    0  /* full speed */

extern PCD_HandleTypeDef hpcd_USB_FS;

/* Simple static memory management */
void *USBD_static_malloc(uint32_t size);
void USBD_static_free(void *p);

#define USBD_malloc         (uint32_t*)USBD_static_malloc
#define USBD_free           USBD_static_free


#ifdef __cplusplus
}
#endif

#endif
