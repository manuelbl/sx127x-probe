/*
 * LMIC Probe - STM32F1x software to monitor LMIC LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Timing functions
 */

#include "timing.h"


volatile uint32_t UptimeMillis;


void SysTick_Handler()
{
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_13);
    UptimeMillis++;
    HAL_IncTick();
}
