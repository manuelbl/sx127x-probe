/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Asynchronous UART/serial output
 */

#ifndef UART_H
#define UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

class UartImpl
{
public:
    void Init();
    void Write(const uint8_t *data, size_t len);
    void Print(const char *str);
    void PrintHex(const uint8_t *data, size_t len, _Bool crlf);

    static void TransmissionCompleted();

private:
    static void StartTransmit();
    static bool TryAppend(int bufHead);
};

extern UartImpl Uart;

#endif
