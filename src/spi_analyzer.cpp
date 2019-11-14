/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * SPI communication analyzer
 */

#include <stdbool.h>
#include "main.h"
#include "spi_analyzer.h"
#include "uart.h"


void SpiAnalyzer::analyzeTrx(uint32_t time, const uint8_t* startTrx, const uint8_t* endTrx,
        const uint8_t* startBuf, const uint8_t* endBuf)
{
    const uint8_t* p = startTrx;
    uint8_t reg = *p;

    // check for write to register
    if ((reg & 0x80) == 0)
        return;
    
    reg = reg & 0x7f;

    p++;
    if (p == endBuf)
        p = startBuf;

    // save register value
    uint8_t value = *p;

    p++;
    if (p == endBuf)
        p = startBuf;

    // check for transaction length
    if (p != endTrx)
        return;
    
    processRegWrite(time, reg, value);
}


void SpiAnalyzer::processRegWrite(uint32_t time, uint8_t reg, uint8_t value)
{
    switch (reg)
    {
        case 1: // OPMODE
            processOpmodeChange(time, value);
            break;
        default:
            break;
    }
}


void SpiAnalyzer::processOpmodeChange(uint32_t time, uint8_t value)
{
    // Check for mode RXSINGLE and TX, respectively
    _Bool isRxStart = (value & 0x07) == 0x06;
    _Bool isTxStart = (value & 0x07) == 0x03;
    if (!isRxStart && !isTxStart)
        return;

    PrintTimestamp(time);
    uartPrint(isRxStart ? "RX start\r\n" : "TX start\r\n");
}
