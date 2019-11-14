/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Asynchronous UART/serial output
 */

#ifndef _UART_H_
#define _UART_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

class UartImpl
{
public:
    void Init();
    void Write(const uint8_t* data, size_t len);
    void Print(const char* str);
    void PrintHex(const uint8_t* data, size_t len, _Bool crlf);

    void StartTransmit();
};

extern UartImpl Uart;

#endif
