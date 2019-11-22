/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Main functions
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stm32f1xx.h>


void ErrorHandler();


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
