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

#define DIO0_PIN GPIO_PIN_3
#define DIO0_GPIO_PORT GPIOB
#define EXTI_DIO0_IRQn EXTI3_IRQn
#define EXTI_DIO0_IRQHandler EXTI3_IRQHandler

#define DIO1_PIN GPIO_PIN_4
#define DIO1_GPIO_PORT GPIOB
#define EXTI_DIO1_IRQn EXTI4_IRQn
#define EXTI_DIO1_IRQHandler EXTI4_IRQHandler

#define SPI_INSTANCE SPI2
#define SPI_PORT GPIOB
#define SPI_NSS_PIN GPIO_PIN_12
#define SPI_SCK_PIN GPIO_PIN_13
#define SPI_MOSI_PIN GPIO_PIN_15
#define DMA_SPI_Instance DMA1_Channel4
#define DMA_SPI_IRQn DMA1_Channel4_IRQn
#define DMA_SPI_IRQHandler DMA1_Channel4_IRQHandler
#define EXTI_NSS_IRQn EXTI15_10_IRQn
#define EXTI_NSS_IRQHandler EXTI15_10_IRQHandler


#if !defined(SPI_MODE)
#define SPI_MODE 0
#endif

extern SPI_HandleTypeDef hspi;
extern DMA_HandleTypeDef hdma_spi_rx;

void DioTriggered(int dio);
void SpiTrxCompleted();

void setup();

#endif
