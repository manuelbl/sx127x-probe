/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Timing functions
 */

#include "timing.h"


volatile uint32_t UptimeMillis;


extern "C" void SysTick_Handler()
{
    UptimeMillis++;
    HAL_IncTick();
}
