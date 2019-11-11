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
#include "setup.h"
#include "uart.h"


typedef enum {
    eventSpiTrx,
    eventDio0,
    eventDio1
} EventType;


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
static volatile int spiTrxDataEnd[EVENT_QUEUE_LEN];
static volatile int eventQueueHead = 0;
static volatile int eventQueueTail = 0;

void Error_Handler();
static void TestForRxTxStart(uint8_t* start, uint8_t* end);

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
            uint8_t* start = spiDataBuf + spiTrxDataEnd[tail];
            tail++;
            if (tail >= EVENT_QUEUE_LEN)
                tail = 0;
            
            switch (eventType)
            {
                case eventSpiTrx:
                    TestForRxTxStart(start, spiDataBuf + spiTrxDataEnd[tail]);
                    break;
                case eventDio0:
                case eventDio1:
                    uartPrint(eventType == eventDio0 ? "Done\r\n" : "Timeout\r\n");
                    break;
            }

            eventQueueTail = tail;
        }
    }
}

static void TestForRxTxStart(uint8_t* start, uint8_t* end)
{
    uint8_t* p = start;
    _Bool isRxStart = *p == 0x81;
    _Bool isTxStart = isRxStart;
    p++;
    if (p == spiDataBuf + SPI_DATA_BUF_LEN)
        p = spiDataBuf;
    isRxStart = isRxStart && (*p & 0x07) == 0x06;
    isTxStart = isTxStart && (*p & 0x07) == 0x03;
    p++;
    if (p == spiDataBuf + SPI_DATA_BUF_LEN)
        p = spiDataBuf;
    isRxStart = isRxStart && p == end;
    isTxStart = isTxStart && p == end;

    if (isRxStart)
        uartPrint("RX start\r\n");
    if (isTxStart)
        uartPrint("TX start\r\n");
}

void DioTriggered(int dio)
{
    int head = eventQueueHead;
    int spiPos = spiTrxDataEnd[head];
    head++;
    if (head >= EVENT_QUEUE_LEN)
        head = 0;
    if (head == eventQueueTail)
        return;

    eventTypes[head] = dio == 0 ? eventDio0 : eventDio1;
    spiTrxDataEnd[head] = spiPos;
    eventQueueHead = head;
}

void SpiTrxCompleted()
{
    int pos = SPI_DATA_BUF_LEN - __HAL_DMA_GET_COUNTER(&hdma_spi1_rx);
    if (pos == SPI_DATA_BUF_LEN)
        pos = 0;

    int head = eventQueueHead + 1;
    if (head >= EVENT_QUEUE_LEN)
        head = 0;
    if (head != eventQueueTail)
    {
        eventTypes[head] = eventSpiTrx;
        spiTrxDataEnd[head] = pos;
        eventQueueHead = head;
    }
}
