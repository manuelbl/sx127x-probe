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
static SpiAnalyzer spiAnalyzer(timingAnalyzer);

void Error_Handler();

int main()
{
    setup();

    Uart.Print("Start\r\n");

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
                spiAnalyzer.OnTrx(time, start, spiDataBuf + spiTrxDataEnd[tail],
                                       spiDataBuf, spiDataBuf + SPI_DATA_BUF_LEN);
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
        return;

    eventTypes[head] = eventType;
    eventTime[head] = us;
    spiTrxDataEnd[head] = spiPos;
    eventQueueHead = head;
}

void SpiTrxCompleted()
{
    int pos = SPI_DATA_BUF_LEN - __HAL_DMA_GET_COUNTER(&hdma_spi1_rx);
    if (pos == SPI_DATA_BUF_LEN)
        pos = 0;

    QueueEvent(EventTypeSpiTrx, pos);
}
