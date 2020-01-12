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

#include "common.h"
#include <stdint.h>
#include <stm32f1xx.h>

#if defined(UART_OUTPUT)
    #define Serial Uart
    #include "uart.h"
#else
    #define Serial USBSerial
    #include "usb_serial.h"
#endif

#endif
