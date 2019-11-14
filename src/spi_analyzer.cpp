/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * SPI communication analyzer
 */

#include "main.h"
#include "spi_analyzer.h"


void SpiAnalyzer::AnalyzeTrx(uint32_t time, const uint8_t* startTrx, const uint8_t* endTrx,
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
    
    ProcessRegWrite(time, reg, value);
}


void SpiAnalyzer::ProcessRegWrite(uint32_t time, uint8_t reg, uint8_t value)
{
    switch (reg)
    {
        case 1: // OPMODE
            ProcessOpmodeChange(time, value);
            break;
        default:
            break;
    }
}


void SpiAnalyzer::ProcessOpmodeChange(uint32_t time, uint8_t value)
{
    uint8_t mode = value & 0x07;
    if (mode == 0x03)
    {
        timingAnalyzer.StartTx(time);
    }
    else if (mode == 0x06)
    {
        timingAnalyzer.StartRx(time);
    }
}
