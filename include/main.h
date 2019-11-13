/*
 * LMIC Probe - STM32F1x software to monitor LMIC LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Main functions
 */

#ifndef _MAIN_H_
#define _MAIN_H_

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


#ifdef __cplusplus
}
#endif

#endif
