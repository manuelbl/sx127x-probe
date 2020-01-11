/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * USB Serial Communication
 */

#ifndef USB_SERIAL_H
#define USB_SERIAL_H

#include <cstdbool>
#include <cstddef>
#include <cstdint>


class USBSerialImpl
{
public:
    void Init();
    void Write(const uint8_t *data, size_t len);
    void Print(const char *str);
    void Printf(const char *fmt, ...);
    void PrintHex(const uint8_t *data, size_t len, _Bool crlf);

    bool IsTxIdle();
    bool IsConnected();

private:
    void Reset();

    static bool FlushTxBuffer();
    static bool TryAppend(int bufHead);
    static void StartTransmit();
    static void TransmissionCompleted();
    static void InstallDataInSerial();

    static uint8_t DataInSerial(void* dev, uint8_t epnum);

    static int8_t CDCInit();
    static int8_t CDCDeInit();
    static int8_t CDCControl(uint8_t cmd, uint8_t* pbuf, uint16_t length);
    static int8_t CDCReceive(uint8_t* pbuf, uint32_t *Len);
};

extern USBSerialImpl USBSerial;


#endif
