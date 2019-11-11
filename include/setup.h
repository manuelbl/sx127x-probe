/*
 * LMIC Probe - STM32F1x software to monitor LMIC LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Peripheral setup and initialization (incl. interrupts)
 */

#ifndef _SETUP_H_
#define _SETUP_H_

#include <stm32f1xx_hal.h>

#define DIO0_PIN GPIO_PIN_0
#define DIO0_GPIO_PORT GPIOB
#define DIO1_PIN GPIO_PIN_1
#define DIO1_GPIO_PORT GPIOB

extern SPI_HandleTypeDef hspi1;
extern DMA_HandleTypeDef hdma_spi1_rx;

void DioTriggered(int dio);
void SpiTrxCompleted();

void setup();

#endif
