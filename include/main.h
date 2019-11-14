/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Main functions
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
    EventTypeSpiTrx,
    EventTypeDone,
    EventTypeTimeout
} EventType;


void QueueEvent(EventType eventType, int spiPos);
void PrintTimestamp(uint32_t timestamp);


#ifdef __cplusplus
}
#endif

#endif