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
#include <cstring>
#include <stm32f1xx_hal.h>

UartImpl Uart;

static UART_HandleTypeDef huart1;
static DMA_HandleTypeDef hdma_usart1_tx;

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

#define HEX_BUF_LEN 25
static char hexBuf[HEX_BUF_LEN];

static const char *HEX_DIGITS = "0123456789ABCDEF";

void UartImpl::Print(const char *str)
{
    Write((const uint8_t *)str, strlen(str));
}

void UartImpl::PrintHex(const uint8_t *data, size_t len, _Bool crlf)
{
    while (len > 0)
    {
        char *p = hexBuf;
        char *end = hexBuf + HEX_BUF_LEN;

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

        Write((const uint8_t *)hexBuf, p - hexBuf);
    }
}

void UartImpl::Write(const uint8_t *data, size_t len)
{
    int bufTail = txBufTail;
    int bufHead = txBufHead;
    if (bufHead == bufTail && txQueueHead != txQueueTail)
    {
        // tx data buffer is full
        ErrorHandler();
        return;
    }

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

    if (huart1.gState != HAL_UART_STATE_READY || txQueueTail == txQueueHead)
        return; // UART busy or queue empty

    int queueTail = txQueueTail;
    int endPos = txChunkBreak[queueTail];
    if (endPos == 0)
        endPos = TX_BUF_LEN;
    queueTail--;
    if (queueTail < 0)
        queueTail = TX_QUEUE_LEN - 1;
    int startPos = txChunkBreak[queueTail];

    if (startPos < 0 || startPos >= TX_BUF_LEN
            || endPos <= 0 || endPos > TX_BUF_LEN
            || endPos <= startPos)
    {
        ErrorHandler();
    }

    HAL_UART_Transmit_DMA(&huart1, txBuf + startPos, endPos - startPos);
}

void UartImpl::TransmissionCompleted()
{
    {
        InterruptGuard guard;

        int queueTail = txQueueTail;
        txBufTail = txChunkBreak[queueTail]; 
        
        queueTail++;
        if (queueTail >= TX_QUEUE_LEN)
            queueTail = 0;
        txQueueTail = queueTail;
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
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);

    // DMA1_Channel4_IRQn interrupt configuration
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

    txChunkBreak[txQueueHead] = txBufHead;
}

extern "C" void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (huart->Instance == USART1)
    {
        // Peripheral clock enable
        __HAL_RCC_USART1_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        // USART1 GPIO Configuration
        // PA9     ------> USART1_TX
        // PA10     ------> USART1_RX
        GPIO_InitStruct.Pin = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        // USART1 DMA Init
        // USART1_TX Init
        hdma_usart1_tx.Instance = DMA1_Channel4;
        hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
        HAL_DMA_Init(&hdma_usart1_tx);

        __HAL_LINKDMA(huart, hdmatx, hdma_usart1_tx);

        // USART interrupt is needed for completion callback
        HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
}

extern "C" void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        // Peripheral clock disable
        __HAL_RCC_USART1_CLK_DISABLE();

        // USART1 GPIO Configuration
        // PA9     ------> USART1_TX
        // PA10     ------> USART1_RX
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);

        // USART1 DMA DeInit
        HAL_DMA_DeInit(huart->hdmatx);

        HAL_NVIC_DisableIRQ(USART1_IRQn);
    }
}

extern "C" void DMA1_Channel4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

extern "C" void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
