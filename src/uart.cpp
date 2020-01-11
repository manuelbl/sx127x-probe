/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Asynchronous UART/serial output
 */
#include "uart.h"
#include "main.h"
#include <cstdarg>
#include <cstring>
#include <stm32f1xx_hal.h>

#define UART_RX_PIN GPIO_PIN_3
#define UART_TX_PIN GPIO_PIN_2
#define UART_PORT GPIOA
#define UART_Instance USART2
#define UART_IRQn USART2_IRQn
#define UART_IRQHandler USART2_IRQHandler

#define DMA_UART_Channel DMA1_Channel7
#define DMA_UART_IRQn DMA1_Channel7_IRQn
#define DMA_UART_IRQHandler DMA1_Channel7_IRQHandler


UartImpl Uart;

static UART_HandleTypeDef uart;
static DMA_HandleTypeDef hdma_uart_tx;

// Buffer for data to be transmitted via UART
//  *  0 <= head < buf_len
//  *  0 <= tail < buf_len
//  *  head == tail => empty or full
// Whether the buffer is empty or full needs to be derived
// from the data chunk queue: the buffer is empty if the
// chunk queue is empty.
// `txBufHead` points to the positions where the next character
// should be inserted. `txBufTail` points to the character after
// the last character that has been transmitted.
#define TX_BUF_LEN 1024
static uint8_t txBuf[TX_BUF_LEN];
static volatile int txBufHead = 0;
static volatile int txBufTail = 0;

// Queue of UART transmission data chunks:
//  *  0 <= head < queue_len
//  *  0 <= tail < queue_len
//  *  head == tail => empty
//  *  head + 1 == tail => full (modulo TX_QUEUE_LEN)
// `txQueueHead` points to the position where the next item must be added.
// `txQueueTail` points to the next item that needs to be processed
// or is being processed.
// With current item's index, `txChunkBreak` points to the end
// of the data to be transmitted. The start can be retrieved with
// index - 1 (modulo TX_QUEUE_LEN).
#define TX_QUEUE_LEN 16
static volatile int txChunkBreak[TX_QUEUE_LEN];
static volatile int txQueueHead = 0;
static volatile int txQueueTail = 0;

static char formatBuf[128];

static const char *HEX_DIGITS = "0123456789ABCDEF";

void UartImpl::Print(const char *str)
{
    Write((const uint8_t *)str, strlen(str));
}

void UartImpl::Printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(formatBuf, sizeof(formatBuf), fmt, args);
    va_end(args);
    Print(formatBuf);
}

void UartImpl::PrintHex(const uint8_t *data, size_t len, _Bool crlf)
{
    while (len > 0)
    {
        char *p = formatBuf;
        char *end = formatBuf + sizeof(formatBuf);

        while (len > 0 && p + 4 <= end)
        {
            uint8_t byte = *data++;
            *p++ = HEX_DIGITS[byte >> 4U];
            *p++ = HEX_DIGITS[byte & 0xfU];
            *p++ = ' ';
            len--;
        }

        if (len == 0 && crlf)
        {
            p[-1] = '\r';
            *p++ = '\n';
        }

        Write((const uint8_t *)formatBuf, p - formatBuf);
    }
}

void UartImpl::Write(const uint8_t *data, size_t len)
{
    int bufTail = txBufTail;
    int bufHead = txBufHead;
    if (bufHead == bufTail && txQueueHead != txQueueTail)
        return; // tx data buffer is full - discard data

    size_t availChunkSize = bufHead < bufTail ? bufTail - bufHead : TX_BUF_LEN - bufHead;

    // Copy data to transmit buffer
    size_t size = len <= availChunkSize ? len : availChunkSize;
    memcpy(txBuf + bufHead, data, size);
    bufHead += size;
    if (bufHead >= TX_BUF_LEN)
        bufHead = 0;

    // try to increase existing chunk
    // if it's not being transmitted yet

    if (!TryAppend(bufHead))
    {
        // create new chunk
        int queueHead = txQueueHead;
        txChunkBreak[queueHead] = bufHead;

        queueHead++;
        if (queueHead >= TX_QUEUE_LEN)
            queueHead = 0;
        if (queueHead == txQueueTail)
        {
            // chunk queue is full
            ErrorHandler();
            return;
        }

        txBufHead = bufHead;
        txQueueHead = queueHead;
    }

    // start transmission
    StartTransmit();

    if (size < len)
        Write(data + size, len - size);
}

bool UartImpl::TryAppend(int bufHead)
{
    // Try to append to newest pending chunk,
    // provided it's not yet being transmitted

    InterruptGuard guard;
    
    int queueTail = txQueueTail;
    int queueHead = txQueueHead;
    if (queueTail == queueHead)
        return false; // no pending chunk

    queueTail++;
    if (queueTail >= TX_QUEUE_LEN)
        queueTail = 0;
    if (queueTail == queueHead)
        return false; // a single chunk (already being transmitted)

    queueHead--;
    if (queueHead < 0)
        queueHead = TX_QUEUE_LEN - 1;

    if (txChunkBreak[queueHead] == 0)
        return false; // non-contiguous chunk

    txBufHead = bufHead;
    txChunkBreak[queueHead] = bufHead;

    return true;
}

void UartImpl::StartTransmit()
{
    InterruptGuard guard;

    if (uart.gState != HAL_UART_STATE_READY || txQueueTail == txQueueHead)
        return; // UART busy or queue empty

    int startPos = txBufTail;
    int endPos = txChunkBreak[txQueueTail];
    if (endPos == 0)
        endPos = TX_BUF_LEN;

    HAL_UART_Transmit_DMA(&uart, txBuf + startPos, endPos - startPos);
}

void UartImpl::TransmissionCompleted()
{
    {
        InterruptGuard guard;

        int queueTail = txQueueTail;
        txBufTail = txChunkBreak[queueTail]; 
        txQueueTail++;
        if (txQueueTail >= TX_QUEUE_LEN)
            txQueueTail = 0;
    }

    Uart.StartTransmit();
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    Uart.TransmissionCompleted();
}


// --- Initialization (mostly generated by STM32 CubeMX)

void UartImpl::Init()
{
    uart.Instance = UART_Instance;
    uart.Init.BaudRate = 115200;
    uart.Init.WordLength = UART_WORDLENGTH_8B;
    uart.Init.StopBits = UART_STOPBITS_1;
    uart.Init.Parity = UART_PARITY_NONE;
    uart.Init.Mode = UART_MODE_TX;
    uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&uart);

    // DMA UART IRQn interrupt configuration
    HAL_NVIC_SetPriority(DMA_UART_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA_UART_IRQn);

    txChunkBreak[txQueueHead] = txBufHead;
}

extern "C" void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (huart->Instance == UART_Instance)
    {
        // Peripheral clock enable
        __HAL_RCC_USART2_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        // USART2 GPIO Configuration
        // PA2     ------> USART2_TX
        // PA3     ------> USART2_RX
        GPIO_InitStruct.Pin = UART_TX_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(UART_PORT, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = UART_RX_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(UART_PORT, &GPIO_InitStruct);

        // USART2 DMA Init
        // USART2_TX Init
        hdma_uart_tx.Instance = DMA_UART_Channel;
        hdma_uart_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_uart_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_uart_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_uart_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_uart_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_uart_tx.Init.Mode = DMA_NORMAL;
        hdma_uart_tx.Init.Priority = DMA_PRIORITY_LOW;
        HAL_DMA_Init(&hdma_uart_tx);

        __HAL_LINKDMA(huart, hdmatx, hdma_uart_tx);

        // USART interrupt is needed for completion callback
        HAL_NVIC_SetPriority(UART_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(UART_IRQn);
    }
}

extern "C" void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART_Instance)
    {
        // Peripheral clock disable
        __HAL_RCC_USART2_CLK_DISABLE();

        // USART2 GPIO Configuration
        // PA2     ------> USART2_TX
        // PA3     ------> USART3_RX
        HAL_GPIO_DeInit(UART_PORT, UART_TX_PIN | UART_RX_PIN);

        // USART2 DMA DeInit
        HAL_DMA_DeInit(huart->hdmatx);

        HAL_NVIC_DisableIRQ(UART_IRQn);
    }
}

extern "C" void DMA_UART_IRQHandler()
{
    HAL_DMA_IRQHandler(&hdma_uart_tx);
}

extern "C" void UART_IRQHandler()
{
    HAL_UART_IRQHandler(&uart);
}
