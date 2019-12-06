/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Peripheral setup and initialization (incl. interrupts)
 */

#ifndef SETUP_H
#define SETUP_H

#include <stm32f1xx_hal.h>

#define DIO0_PIN GPIO_PIN_0
#define DIO0_GPIO_PORT GPIOB
#define DIO1_PIN GPIO_PIN_1
#define DIO1_GPIO_PORT GPIOB

#if !defined(SPI_MODE)
#define SPI_MODE 0
#endif

extern SPI_HandleTypeDef hspi1;
extern DMA_HandleTypeDef hdma_spi1_rx;

void DioTriggered(int dio);
void SpiTrxCompleted();

void setup();

#endif
