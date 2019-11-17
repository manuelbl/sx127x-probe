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


const uint32_t BANDWIDTH_TABLE[] = {
    7800,
    10400,
    15600,
    20800,
    31250,
    41700,
    62500,
    125000,
    250000,
    500000
};

void SpiAnalyzer::OnTrx(uint32_t time, const uint8_t *startTrx, const uint8_t *endTrx)
{
    const uint8_t *p = startTrx;
    uint8_t reg = *p;

    // check for write to register
    if ((reg & 0x80) == 0)
        return;

    reg = reg & 0x7f;

    p++;
    if (p == circularBufferEnd)
        p = circularBufferStart;

    // save register value
    uint8_t value = *p;

    p++;
    if (p == circularBufferEnd)
        p = circularBufferStart;

    // check for transaction length
    if (p != endTrx)
        return;

    OnRegWrite(time, reg, value);
}

void SpiAnalyzer::OnRegWrite(uint32_t time, uint8_t reg, uint8_t value)
{
    switch (reg)
    {
    case 0x01: // OpMode
        OnOpModeChanged(time, value);
        break;
    case 0x1d: // ModemConfig1
        OnModemConfig1(value);
        break;
    case 0x1e: // ModemConfig2
        OnModemConfig2(value);
        break;
    case 0x1f: // SymbTimeoutLsb
        OnSymbTimeoutLsbChanged(value);
        break;
    case 0x20: // PreambleMsb
        OnPreambleMsbChanged(value);
        break;
    case 0x21: // PreambleMsb
        OnPreambleLsbChanged(value);
        break;
    case 0x22: // PayloadLength
        OnPayloadLengthChanged(value);
        break;
    case 0x26: // ModemConfig3
        OnPayloadLengthChanged(value);
        break;
    default:
        break;
    }
}

void SpiAnalyzer::OnOpModeChanged(uint32_t time, uint8_t value)
{
    uint8_t mode = value & 0x07;
    if (mode == 0x03)
    {
        timingAnalyzer.OnTxStart(time);
    }
    else if (mode == 0x06)
    {
        timingAnalyzer.OnRxStart(time);
    }
}

void SpiAnalyzer::OnModemConfig1(uint8_t value)
{
    uint8_t bw = value >> 4;
    if (bw >= sizeof(BANDWIDTH_TABLE)/sizeof(BANDWIDTH_TABLE[0]))
        return;
    
    timingAnalyzer.SetBandwidth(BANDWIDTH_TABLE[bw]);

    uint8_t cr = ((value >> 1) & 0x7) + 4;
    if (cr < 5 || cr > 8)
        return;

    timingAnalyzer.SetCodingRate(cr);

    timingAnalyzer.SetImplicitHeader(value & 0x01);
}

void SpiAnalyzer::OnModemConfig2(uint8_t value)
{
    uint8_t sf = value >> 4;
    if (sf < 6 || sf > 12)
        return;
    
    timingAnalyzer.SetSpreadingFactor(sf);

    timingAnalyzer.SetCrcOn((value >> 2) & 0x01);

    *(((uint8_t*)&symbolTimeout) + 1) = value & 0x03;
    timingAnalyzer.SetRxSymbolTimeout(symbolTimeout);
}

void SpiAnalyzer::OnSymbTimeoutLsbChanged(uint8_t value)
{
    *(((uint8_t*)&symbolTimeout) + 0) = value;
    timingAnalyzer.SetRxSymbolTimeout(symbolTimeout);
}

void SpiAnalyzer::OnPreambleMsbChanged(uint8_t value)
{
    *(((uint8_t*)&preambleLength) + 1) = value;
    timingAnalyzer.SetPreambleLength(preambleLength);
}

void SpiAnalyzer::OnPreambleLsbChanged(uint8_t value)
{
    *(((uint8_t*)&preambleLength) + 0) = value;
    timingAnalyzer.SetPreambleLength(preambleLength);
}

void SpiAnalyzer::OnPayloadLengthChanged(uint8_t value)
{
    timingAnalyzer.SetPayloadLength(value);
}

void SpiAnalyzer::OnModemConfig3(uint8_t value)
{
    timingAnalyzer.SetLowDataRateOptimization((value >>3) & 0x001);
}
