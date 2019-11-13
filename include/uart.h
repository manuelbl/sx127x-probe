/*
 * LMIC Probe - STM32F1x software to monitor LMIC LoRa timings
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

#ifdef __cplusplus
extern "C" {
#endif


void uartInit();
void uartWrite(const uint8_t* data, size_t len);
void uartPrint(const char* str);
void uartPrintHex(const uint8_t* data, size_t len, _Bool crlf);


#ifdef __cplusplus
}
#endif

#endif
