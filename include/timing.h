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

#ifdef __cplusplus
extern "C" {
#endif


extern volatile uint32_t UptimeMillis;

static inline uint32_t GetMicros()
{
    uint32_t ms;
    uint32_t st;

    do
    {
        ms = UptimeMillis;
        st = SysTick->VAL;
        asm volatile("nop"); //allow interrupt to fire
        asm volatile("nop");
    } while (ms != UptimeMillis);

    return ms * 1000 - st / ((SysTick->LOAD + 1) / 1000);
}

static inline uint32_t GetMicrosFromISR()
{
    uint32_t st = SysTick->VAL;
    uint32_t pending = SCB->ICSR & SCB_ICSR_PENDSTSET_Msk;
    uint32_t ms = UptimeMillis;

    if (pending == 0)
        ms++;

    return ms * 1000 - st / ((SysTick->LOAD + 1) / 1000);
}


#ifdef __cplusplus
}
#endif

#endif
