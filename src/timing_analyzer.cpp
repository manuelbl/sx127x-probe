/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * LoRa timing analyzer
 */

#include <math.h>
#include <stdio.h>
#include "main.h"
#include "timing_analyzer.h"
#include "uart.h"

#define TIMESTAMP_PATTERN "%10lu: "

static char formatBuf[128];

TimingAnalyzer::TimingAnalyzer()
    : bandwidth(125000), numTimeoutSymbols(0x64), codingRate(5),
        implicitHeader(0), spreadingFactor(7), crcOn(0),
        preambleLength(8), payloadLength(1), lowDataRateOptimization(0)
{
    ResetPhase();
}

void TimingAnalyzer::ResetPhase()
{
    phase = LoraPhaseIdle;
    result = LoraResultNoDownlink;
}

void TimingAnalyzer::OnTxStart(uint32_t time)
{
    if (phase != LoraPhaseIdle)
    {
        Uart.Print("Invalid phase for TX start\r\n");
        ResetPhase();
        return;
    }

    PrintTimestamp(time);
    Uart.Print("TX start\r\n");
    phase = LoraPhaseTransmitting;
    txStartTime = time;
}

void TimingAnalyzer::OnRxStart(uint32_t time)
{
    if (phase != LoraPhaseBeforeRx1Window && phase != LoraPhaseBeforeRx2Window)
    {
        Uart.Print("Invalid phase for RX start\r\n");
        ResetPhase();
        return;
    }

    PrintTimestamp(time);
    Uart.Print("RX start\r\n");

    if (phase == LoraPhaseBeforeRx1Window)
    {
        phase = LoraPhaseInRx1Window;
        rx1Start = time;
    }
    else
    {
        phase = LoraPhaseInRx2Window;
        rx2Start = time;
    }
}

void TimingAnalyzer::OnDoneInterrupt(uint32_t time)
{
    if (phase != LoraPhaseTransmitting
        && phase != LoraPhaseInRx1Window && phase != LoraPhaseInRx2Window)
    {
        Uart.Print("Invalid phase for done interrupt\r\n");
        ResetPhase();
        return;
    }

    PrintTimestamp(time);
    Uart.Print("Done\r\n");

    if (phase == LoraPhaseTransmitting)
    {
        phase = LoraPhaseBeforeRx1Window;
        txEndTime = time;

        uint32_t txExpected = CalculateAirTime();
        uint32_t txEffective = txEndTime - txStartTime;
        snprintf(formatBuf, sizeof(formatBuf), "TX: airtime: %ld us (calculated), overall duration: %ld us\r\n",
                txExpected, txEffective);
        Uart.Print(formatBuf);
    }
    else if (phase == LoraPhaseInRx1Window)
    {
        rx1End = time;
        result = LoraResultDownlinkInRx1;
        OnRxTxCompleted();
    }
    else
    {
        rx2End = time;
        result = LoraResultDownlinkInRx2;
        OnRxTxCompleted();
    }
}

void TimingAnalyzer::OnTimeoutInterrupt(uint32_t time)
{
    if (phase != LoraPhaseInRx1Window && phase != LoraPhaseInRx2Window)
    {
        Uart.Print("Invalid phase for timeout interrupt\r\n");
        ResetPhase();
        return;
    }

    uint32_t txExpected = CalculateTimeoutTime();
    uint32_t txEffective;

    PrintTimestamp(time);
    Uart.Print("Timeout\r\n");

    if (phase == LoraPhaseInRx1Window)
    {
        phase = LoraPhaseBeforeRx2Window;
        rx1End = time;
        txEffective = time - rx1Start;

    }
    else
    {
        rx2End = time;
        result = LoraResultNoDownlink;
        txEffective = time - rx2Start;
        OnRxTxCompleted();
    }

    snprintf(formatBuf, sizeof(formatBuf), "RX timeout: expected: %ld us, effective: %ld us\r\n",
            txExpected, txEffective);
    Uart.Print(formatBuf);
}

void TimingAnalyzer::OnRxTxCompleted()
{
    phase = LoraPhaseIdle;
}

void TimingAnalyzer::PrintTimestamp(uint32_t timestamp)
{
    timestamp = (uint32_t)lround(timestamp * TIMING_CORR);
    char buf[sizeof(TIMESTAMP_PATTERN) + 7];
    snprintf(buf, sizeof(buf), TIMESTAMP_PATTERN, timestamp);
    Uart.Print(buf);
}

uint32_t TimingAnalyzer::CalculateAirTime()
{
    uint32_t symbolDuration = (1 << spreadingFactor) * 1000000 / bandwidth;
    uint32_t preambleDuration = 12 * symbolDuration + symbolDuration / 4;

    uint8_t div = 4 * (spreadingFactor - 2 * lowDataRateOptimization);
    uint32_t numPayloadSymbols
            = (8 * payloadLength - 4 * spreadingFactor + 28 + 16 - 20 * implicitHeader + div - 1) / div;
    numPayloadSymbols *= codingRate;
    if (numPayloadSymbols < 0)
        numPayloadSymbols = 0;
    numPayloadSymbols += 8;
    uint32_t payloadDuration = numPayloadSymbols * symbolDuration;

    return preambleDuration + payloadDuration;
}

uint32_t TimingAnalyzer::CalculateTimeoutTime()
{
    uint32_t symbolDuration = (1 << spreadingFactor) * 1000000 / bandwidth;
    return symbolDuration * numTimeoutSymbols;
}