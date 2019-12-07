/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Main code (SPI data decoding, output, most initialization)
 */
#include "main.h"
#include "setup.h"
#include "spi_analyzer.h"
#include "timing.h"
#include "timing_analyzer.h"
#include "uart.h"
#include <cstring>

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
//  *  head + 1 == tail => full (modulo EVENT_QUEUE_LEN)
// `head` points to the position where the next item must be added.
// `tail` points to the next item that needs to be processed.
// For event type `EventTypeSpiTrx`, the item's index can be used
// to retrieve the end of the SPI data. The start of the SPI data
// is at index - 1 (modulo EVENT_QUEUE_LEN).
#define EVENT_QUEUE_LEN 16
static volatile EventType eventTypes[EVENT_QUEUE_LEN];
static volatile uint32_t eventTime[EVENT_QUEUE_LEN];
static volatile int spiTrxDataEnd[EVENT_QUEUE_LEN];
static volatile int eventQueueHead = 0;
static volatile int eventQueueTail = 0;
static volatile uint8_t eventQueueOverflow = 0;

static TimingAnalyzer timingAnalyzer;
static SpiAnalyzer spiAnalyzer(spiDataBuf, SPI_DATA_BUF_LEN, timingAnalyzer);

int main()
{
    setup();

    Uart.Print("SX127x Probe\r\n");

    // Receive SPI data into a circuar buffer indefinitely
    HAL_SPI_Receive_DMA(&hspi, spiDataBuf, SPI_DATA_BUF_LEN);

    while (true)
    {
        if (eventQueueOverflow != 0)
        {
            ErrorHandler();
        }
        if (eventQueueHead != eventQueueTail)
        {
            int tail = eventQueueTail;
            EventType eventType = eventTypes[tail];
            uint32_t time = eventTime[tail];
            int prev;

            switch (eventType)
            {
            case EventTypeSpiTrx:
                prev = tail - 1;
                if (prev < 0)
                    prev = EVENT_QUEUE_LEN - 1;
                spiAnalyzer.OnTrx(time, spiDataBuf + spiTrxDataEnd[prev], spiDataBuf + spiTrxDataEnd[tail]);
                break;
            case EventTypeDone:
                timingAnalyzer.OnDoneInterrupt(time);
                break;

            case EventTypeTimeout:
                timingAnalyzer.OnTimeoutInterrupt(time);
                break;
            }

            tail++;
            if (tail >= EVENT_QUEUE_LEN)
                tail = 0;
            eventQueueTail = tail;
        }
    }
}

void QueueEvent(EventType eventType, int spiPos)
{
    uint32_t us = GetMicrosFromISR();
    int head = eventQueueHead;
    if (spiPos == -1)
    {
        int prev = head - 1;
        if (prev < 0)
            prev = EVENT_QUEUE_LEN - 1;
        spiPos = spiTrxDataEnd[prev];
    }

    eventTypes[head] = eventType;
    eventTime[head] = us;
    spiTrxDataEnd[head] = spiPos;

    head++;
    if (head >= EVENT_QUEUE_LEN)
        head = 0;
    if (head == eventQueueTail)
    {
        // queue overflow
        eventQueueOverflow = 1;
        return;
    }

    eventQueueHead = head;
}

// Called when an SPI transaction has completed (NSS returns to HIGH)
void SpiTrxCompleted()
{
    int pos = SPI_DATA_BUF_LEN - __HAL_DMA_GET_COUNTER(&hdma_spi_rx);
    if (pos == SPI_DATA_BUF_LEN)
        pos = 0;

    QueueEvent(EventTypeSpiTrx, pos);
}

// Called when the DIO0 signal goes high
extern "C" void EXTI_DIO0_IRQHandler(void)
{
    QueueEvent(EventTypeDone, -1);
    HAL_GPIO_EXTI_IRQHandler(DIO0_PIN);
}

// Called when the DIO1 signal goes high
extern "C" void EXTI_DIO1_IRQHandler(void)
{
    QueueEvent(EventTypeTimeout, -1);
    HAL_GPIO_EXTI_IRQHandler(DIO1_PIN);
}

void ErrorHandler()
{
    while (true)
    {
    }
}
