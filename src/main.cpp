/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Main code (SPI data decoding, output, most initialization)
 */
#include <string.h>
#include "main.h"
#include "setup.h"
#include "spi_analyzer.h"
#include "timing.h"
#include "timing_analyzer.h"
#include "uart.h"

enum EventType
{
    EventTypeSpiTrx,
    EventTypeDone,
    EventTypeTimeout
};

// Buffer for payload data of SPI transaction.
// The buffer is used as a circular buffer.
#define SPI_DATA_BUF_LEN 128
static uint8_t spiDataBuf[128];

// Queue of SPI transactions:
//  *  0 <= head < queue_len
//  *  0 <= tail < queue_len
//  *  head == tail => empty
//  *  head + 1 == tail => full (mod queue_len)
#define EVENT_QUEUE_LEN 16
static volatile EventType eventTypes[EVENT_QUEUE_LEN];
static volatile uint32_t eventTime[EVENT_QUEUE_LEN];
static volatile int spiTrxDataEnd[EVENT_QUEUE_LEN];
static volatile int eventQueueHead = 0;
static volatile int eventQueueTail = 0;

static TimingAnalyzer timingAnalyzer;
static SpiAnalyzer spiAnalyzer(spiDataBuf, SPI_DATA_BUF_LEN, timingAnalyzer);

int main()
{
    setup();

    Uart.Print("Start\r\n");

    // Receive SPI data into a circuar buffer indefinitely
    HAL_SPI_Receive_DMA(&hspi1, spiDataBuf, SPI_DATA_BUF_LEN);

    while (1)
    {
        if (eventQueueHead != eventQueueTail)
        {
            int tail = eventQueueTail;
            EventType eventType = eventTypes[tail];
            uint8_t *start = spiDataBuf + spiTrxDataEnd[tail];
            uint32_t time = eventTime[tail];
            tail++;
            if (tail >= EVENT_QUEUE_LEN)
                tail = 0;

            switch (eventType)
            {
            case EventTypeSpiTrx:
                spiAnalyzer.OnTrx(time, start, spiDataBuf + spiTrxDataEnd[tail]);
                break;

            case EventTypeDone:
                timingAnalyzer.OnDoneInterrupt(time);
                break;

            case EventTypeTimeout:
                timingAnalyzer.OnTimeoutInterrupt(time);
                break;
            }

            eventQueueTail = tail;
        }
    }
}

void QueueEvent(EventType eventType, int spiPos)
{
    uint32_t us = GetMicrosFromISR();
    int head = eventQueueHead;
    if (spiPos == -1)
        spiPos = spiTrxDataEnd[head];

    head++;
    if (head >= EVENT_QUEUE_LEN)
        head = 0;
    if (head == eventQueueTail)
    {
        // queue overrun
        ErrorHandler();
        return;
    }

    eventTypes[head] = eventType;
    eventTime[head] = us;
    spiTrxDataEnd[head] = spiPos;
    eventQueueHead = head;
}

// Called when an SPI transaction has completed (NSS returns to HIGH)
void SpiTrxCompleted()
{
    int pos = SPI_DATA_BUF_LEN - __HAL_DMA_GET_COUNTER(&hdma_spi1_rx);
    if (pos == SPI_DATA_BUF_LEN)
        pos = 0;

    QueueEvent(EventTypeSpiTrx, pos);
}

// Called when the DIO0 signal goes high
extern "C" void EXTI0_IRQHandler(void)
{
    QueueEvent(EventTypeDone, -1);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

// Called when the DIO1 signal goes high
extern "C" void EXTI1_IRQHandler(void)
{
    QueueEvent(EventTypeTimeout, -1);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}

void ErrorHandler()
{
    while (1)
    {
    }
}
