/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Common definitions shared between libraries and main code
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stm32f1xx.h>


extern "C" void Error_Handler();
extern "C" void ErrorHandler();


class InterruptGuard {
public:
    InterruptGuard() {
        __disable_irq();
    }

    ~InterruptGuard() {
        __enable_irq();
    }
};

#endif
