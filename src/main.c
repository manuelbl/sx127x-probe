/*
 * LMIC Probe - STM32F1x software to monitor LMIC LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Main code (SPI data decoding, output, most initialization)
 */
#include <string.h>
#include <stdio.h>
#include "setup.h"
#include "uart.h"
#include "timing.h"
#include "main.h"


#define TIMESTAMP_PATTERN "%10lu: "

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

static char timestampBuf[20];


void Error_Handler();
static void TestForRxTxStart(uint32_t time, uint8_t *start, uint8_t *end);

int main()
{
    setup();

    uartPrint("Start\r\n");

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
                TestForRxTxStart(time, start, spiDataBuf + spiTrxDataEnd[tail]);
                break;
            case EventTypeDone:
            case EventTypeTimeout:
                snprintf(timestampBuf, sizeof(timestampBuf), TIMESTAMP_PATTERN, time);
                uartPrint(timestampBuf);
                uartPrint(eventType == EventTypeDone ? "Done\r\n" : "Timeout\r\n");
                break;
            }

            eventQueueTail = tail;
        }
    }
}

static void TestForRxTxStart(uint32_t time, uint8_t *start, uint8_t *end)
{
    uint8_t *p = start;

    // check for write to OPMODE register
    if (*p != 0x81)
        return;

    p++;
    if (p == spiDataBuf + SPI_DATA_BUF_LEN)
        p = spiDataBuf;
    
    // Check for mode RXSINGLE and TX, respectively
    _Bool isRxStart = (*p & 0x07) == 0x06;
    _Bool isTxStart = (*p & 0x07) == 0x03;
    if (!isRxStart && !isTxStart)
        return;

    p++;
    if (p == spiDataBuf + SPI_DATA_BUF_LEN)
        p = spiDataBuf;
    
    // Check for a complete SPI transaction
    if (p != end)
        return;

    snprintf(timestampBuf, sizeof(timestampBuf), TIMESTAMP_PATTERN, time);
    uartPrint(timestampBuf);
    uartPrint(isRxStart ? "RX start\r\n" : "TX start\r\n");
}


void QueueEvent(EventType eventType, int spiPos)
{
    uint32_t us = GetMicros();
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
