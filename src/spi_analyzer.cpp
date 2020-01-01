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
#include "uart.h"


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
#if SPI_DEBUG == 1
    if (endTrx > startTrx)
    {
        Uart.PrintHex(startTrx, endTrx - startTrx, true);
    }
    else
    {
        Uart.PrintHex(startTrx, circularBufferEnd - startTrx, false);
        Uart.PrintHex(circularBufferStart, endTrx - circularBufferStart, true);
    }
#endif

    const uint8_t *p = startTrx;
    uint8_t reg = *p;

    // check for FIFO read
    if (reg == 0x00)
    {
        OnFifoRead(startTrx, endTrx);
        return;
    }

    // check for write to register
    if ((reg & 0x80U) == 0)
        return;

    reg = reg & 0x7fU;

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

void SpiAnalyzer::OnFifoRead(const uint8_t *startTrx, const uint8_t *endTrx)
{
    // FIFO read indicates received data.
    // Interesting information is length of data.
    if (endTrx < startTrx)
        endTrx += circularBufferEnd - circularBufferStart;
    uint8_t len = endTrx - startTrx - 1;
    timingAnalyzer.OnDataReceived(len);
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
        OnModemConfig3(value);
        break;
    default:
        break;
    }
}

void SpiAnalyzer::OnOpModeChanged(uint32_t time, uint8_t value)
{
    LongRangeMode longRangeMode = (value & 0x80) != 0 ? LongrangeModeLora : LongrangeModeFSK;
    timingAnalyzer.SetLongRangeMode(longRangeMode);

    uint8_t mode = value & 0x07U;
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
    uint8_t bw = value >> 4U;
    if (bw >= sizeof(BANDWIDTH_TABLE)/sizeof(BANDWIDTH_TABLE[0]))
        return;
    
    timingAnalyzer.SetBandwidth(BANDWIDTH_TABLE[bw]);

    uint8_t cr = ((value >> 1U) & 0x7U) + 4;
    if (cr < 5 || cr > 8)
        return;

    timingAnalyzer.SetCodingRate(cr);

    timingAnalyzer.SetImplicitHeader(value & 0x01U);
}

void SpiAnalyzer::OnModemConfig2(uint8_t value)
{
    uint8_t sf = value >> 4U;
    if (sf >= 6 && sf <= 12)
        timingAnalyzer.SetSpreadingFactor(sf);

    timingAnalyzer.SetCrcOn((value >> 2U) & 0x01U);

    *(((uint8_t*)&symbolTimeout) + 1) = value & 0x03U;
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
    timingAnalyzer.SetTxPayloadLength(value);
}

void SpiAnalyzer::OnModemConfig3(uint8_t value)
{
    timingAnalyzer.SetLowDataRateOptimization((value >> 3U) & 0x001U);
}
