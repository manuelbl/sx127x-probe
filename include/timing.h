/*
 * LMIC Probe - STM32F1x software to monitor LMIC LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Timing functions
 */

#ifndef _TIMING_H_
#define _TIMING_H_

#include <stdint.h>
#include <stm32f1xx_hal.h>

extern volatile uint32_t SystickUptimeMillis;

static inline uint32_t GetMicros()
{
    uint32_t ms;
    uint32_t st;

    do
    {
        ms = SystickUptimeMillis;
        st = SysTick->VAL;
        asm volatile("nop"); //allow interrupt to fire
        asm volatile("nop");
    } while (ms != SystickUptimeMillis);

    return ms * 1000 - st / ((SysTick->LOAD + 1) / 1000);
}

#endif
