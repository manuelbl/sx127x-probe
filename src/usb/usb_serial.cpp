/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * USB Serial Communication
 */

#include "main.h"
#include "usb_serial.h"
#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_ll_gpio.h"
#include "usbd_def.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>


uint8_t (*dataInCDC)(USBD_HandleTypeDef* dev, uint8_t epnum);

static USBD_CDC_ItfTypeDef cdcInterface;

USBSerialImpl USBSerial;

extern PCD_HandleTypeDef hpcd_USB_FS;
static USBD_HandleTypeDef hUsbDevice;

static bool isTransmitting = false;


// Buffer for data to be transmitted via USB Serial
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

// Queue of USB Serial transmission data chunks:
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

// Circular buffer for data received via USB Serial
//  *  0 <= head < < buf_len
//  *  0 <= tail < buf_len
//  *  head == tail => empty
//  *  head + 1 == tail => full (modulo RX_BUF_LEN)
// `rxBufHead` points to the positions where the next received haracter
// should be inserted. `rxBufTail` points to the next character
// that should be passed to the application.
#define RX_BUF_LEN 1024
static uint8_t rxBuf[RX_BUF_LEN];
static volatile int rxBufHead = 0;
static volatile int rxBufTail = 0;

static char formatBuf[128];

static const char *HEX_DIGITS = "0123456789ABCDEF";

void USBSerialImpl::Print(const char *str)
{
    Write((const uint8_t *)str, strlen(str));
}

void USBSerialImpl::Printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(formatBuf, sizeof(formatBuf), fmt, args);
    va_end(args);
    Print(formatBuf);
}

void USBSerialImpl::PrintHex(const uint8_t *data, size_t len, _Bool crlf)
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

void USBSerialImpl::Write(const uint8_t *data, size_t len)
{
    if (txBufHead == txBufTail && txQueueHead != txQueueTail) {
        // tx data buffer is full; flush it
        if (!FlushTxBuffer())
            return; // no space available; discard remaining data
    }

    int bufTail = txBufTail;
    int bufHead = txBufHead;
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
            // chunk queue is full - unlikely to happen
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

bool USBSerialImpl::TryAppend(int bufHead)
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

// Flush buffer.
bool USBSerialImpl::FlushTxBuffer()
{
    InterruptGuard guard;

    if (isTransmitting) {
        // preserve the buffer being transmitted
        txQueueHead = txQueueTail + 1;
        if (txQueueHead >= TX_QUEUE_LEN)
            txQueueHead = 0;
        txBufHead = txChunkBreak[txQueueTail];
        return txBufHead != txBufTail;

    } else {
        txQueueHead = txQueueTail = 0;
        txBufHead = txBufTail = 0;
        return true;
    }
}

void USBSerialImpl::StartTransmit()
{
    InterruptGuard guard;

    if (txQueueTail == txQueueHead
            || !USBSerial.IsConnected()
            || !USBSerial.IsTxIdle())
        return; // Queue empty or USB not connected or USB TX busy

    int startPos = txBufTail;
    int endPos = txChunkBreak[txQueueTail];
    if (endPos == 0)
        endPos = TX_BUF_LEN;

    USBD_CDC_SetTxBuffer(&hUsbDevice, txBuf + startPos, endPos - startPos);
    uint8_t result = USBD_CDC_TransmitPacket(&hUsbDevice);
    if (result != USBD_OK)
        ErrorHandler();
    isTransmitting = true;
}

void USBSerialImpl::TransmissionCompleted()
{
    {
        InterruptGuard guard;

        txBufTail = txChunkBreak[txQueueTail]; 
        txQueueTail++;
        if (txQueueTail >= TX_QUEUE_LEN)
            txQueueTail = 0;

        isTransmitting = false;
    }

    USBSerial.StartTransmit();
}

bool USBSerialImpl::IsTxIdle()
{
    USBD_CDC_HandleTypeDef* hcdc = (USBD_CDC_HandleTypeDef*)hUsbDevice.pClassData;
    return hcdc->TxState == 0;
}

void USBSerialImpl::Init()
{
    cdcInterface = {
        CDCInit,
        CDCDeInit,
        CDCControl,
        CDCReceive
    };
    
    Reset();

    if (USBD_Init(&hUsbDevice, &usbSerialDeviceDescriptor, DEVICE_FS) != USBD_OK)
        ErrorHandler();

    if (USBD_RegisterClass(&hUsbDevice, &USBD_CDC) != USBD_OK)
        ErrorHandler();

    InstallDataInSerial();

    if (USBD_CDC_RegisterInterface(&hUsbDevice, &cdcInterface) != USBD_OK)
        ErrorHandler();

    if (USBD_Start(&hUsbDevice) != USBD_OK)
        ErrorHandler();
}


void USBSerialImpl::Reset()
{
    // Pull USB D+ low for 20ms to trigger a device reenumeration.
    // Since BluePill and the like are missing a proper USB reset circuitry,
    // that's the best that can be done.
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_12, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_12, 0);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_12, LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_SetPinOutputType(GPIOA, LL_GPIO_PIN_12, LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_12);

    HAL_Delay(20);
}


// --- CDC Interface Implementation


int8_t USBSerialImpl::CDCInit()
{
    USBD_CDC_SetTxBuffer(&hUsbDevice, txBuf, 0);
    USBD_CDC_SetRxBuffer(&hUsbDevice, rxBuf);
    return USBD_OK;
}

int8_t USBSerialImpl::CDCDeInit()
{
    return USBD_OK;
}

int8_t USBSerialImpl::CDCControl(uint8_t cmd, uint8_t* buf, uint16_t length)
{
    if (!USBSerial.connected) {
        USBSerial.connected = true;
        StartTransmit();
    }
    return USBD_OK;
}

int8_t USBSerialImpl::CDCReceive(uint8_t* buf, uint32_t *len)
{
    USBD_CDC_SetRxBuffer(&hUsbDevice, &buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDevice);
    return USBD_OK;
}


uint8_t USBSerialImpl::DataInSerial(void* dev, uint8_t epnum)
{
    USBD_HandleTypeDef* pdev = (USBD_HandleTypeDef*)dev;
    uint8_t status = dataInCDC(pdev, epnum);
    if (status == USBD_OK) {
        USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDevice.pClassData;
        if (hcdc->TxState == 0)
            TransmissionCompleted();
    }

    return status;
}

void USBSerialImpl::InstallDataInSerial()
{
    dataInCDC = USBD_CDC.DataIn;
    USBD_CDC.DataIn = (uint8_t (*)(USBD_HandleTypeDef* dev, uint8_t epnum))DataInSerial;
}

extern "C" void USB_LP_CAN1_RX0_IRQHandler()
{
    HAL_PCD_IRQHandler(&hpcd_USB_FS);
}
