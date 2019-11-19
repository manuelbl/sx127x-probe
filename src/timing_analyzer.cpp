/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * LoRa timing analyzer
 */

#include "timing_analyzer.h"
#include "main.h"
#include "uart.h"
#include <cmath>
#include <cstdio>

#define TIMESTAMP_PATTERN "%10lu: "

static char formatBuf[128];

TimingAnalyzer::TimingAnalyzer()
    : stage(LoraStageIdle), result(LoraResultNoDownlink),
      txStartTime(0), txEndTime(0),
      rx1Start(0), rx1End(0), rx2Start(0), rx2End(0),
      bandwidth(125000), numTimeoutSymbols(0x64), codingRate(5),
      implicitHeader(0), spreadingFactor(7), crcOn(0),
      preambleLength(8), txPayloadLength(1), lowDataRateOptimization(0)
{
}

void TimingAnalyzer::ResetStage()
{
    stage = LoraStageIdle;
    result = LoraResultNoDownlink;
}

void TimingAnalyzer::OnTxStart(uint32_t time)
{
    if (stage != LoraStageIdle)
    {
        OutOfSync("TX start");
        return;
    }

    PrintTimestamp(time);
    Uart.Print("TX start\r\n");
    stage = LoraStageTransmitting;
    txStartTime = time;
}

void TimingAnalyzer::OnRxStart(uint32_t time)
{
    if (stage != LoraStageBeforeRx1Window && stage != LoraStageBeforeRx2Window)
    {
        OutOfSync("RX start");
        return;
    }

    PrintTimestamp(time);
    Uart.Print("RX start\r\n");

    if (stage == LoraStageBeforeRx1Window)
    {
        stage = LoraStageInRx1Window;
        rx1Start = time;
    }
    else
    {
        stage = LoraStageInRx2Window;
        rx2Start = time;
    }
}

void TimingAnalyzer::OnDoneInterrupt(uint32_t time)
{
    if (stage != LoraStageTransmitting && stage != LoraStageInRx1Window && stage != LoraStageInRx2Window)
    {
        OutOfSync("done interrupt");
        return;
    }

    PrintTimestamp(time);
    Uart.Print("Done\r\n");

    if (stage == LoraStageTransmitting)
    {
        stage = LoraStageBeforeRx1Window;
        txEndTime = time;

        uint32_t txExpected = CalculateAirTime(txPayloadLength);
        uint32_t txEffective = txEndTime - txStartTime;
        snprintf(formatBuf, sizeof(formatBuf), "TX: airtime: %lu us (calculated), overall duration: %lu us\r\n",
                 txExpected, txEffective);
        Uart.Print(formatBuf);
    }
    else if (stage == LoraStageInRx1Window)
    {
        rx1End = time;
        result = LoraResultDownlinkInRx1;
        stage = LoraStageWaitingForData;
    }
    else
    {
        rx2End = time;
        result = LoraResultDownlinkInRx2;
        stage = LoraStageWaitingForData;
    }
}

void TimingAnalyzer::OnDataReceived(uint8_t payloadLength)
{
    if (stage != LoraStageWaitingForData)
    {
        OutOfSync("reading FIFO");
        return;
    }
    uint32_t rxExpected = CalculateAirTime(payloadLength);
    uint32_t rxEffective = result == LoraResultDownlinkInRx1 ? rx1End - rx1Start : rx2End - rx2Start;
    snprintf(formatBuf, sizeof(formatBuf), "RX: airtime: %lu us (calculated), overall duration: %lu us\r\n",
             rxExpected, rxEffective);
    Uart.Print(formatBuf);

    OnRxTxCompleted();
}

void TimingAnalyzer::OnTimeoutInterrupt(uint32_t time)
{
    if (stage != LoraStageInRx1Window && stage != LoraStageInRx2Window)
    {
        OutOfSync("timeout interrupt");
        return;
    }

    uint32_t txExpected = CalculateTimeoutTime();
    uint32_t txEffective;

    PrintTimestamp(time);
    Uart.Print("Timeout\r\n");

    if (stage == LoraStageInRx1Window)
    {
        stage = LoraStageBeforeRx2Window;
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

    snprintf(formatBuf, sizeof(formatBuf), "RX timeout: expected: %lu us, effective: %lu us\r\n",
             txExpected, txEffective);
    Uart.Print(formatBuf);
}

void TimingAnalyzer::OnRxTxCompleted()
{
    stage = LoraStageIdle;
    result = LoraResultNoDownlink;
}

void TimingAnalyzer::PrintTimestamp(uint32_t timestamp)
{
    timestamp = static_cast<uint32_t>(lround(timestamp * TIMING_CORR));
    char buf[sizeof(TIMESTAMP_PATTERN) + 7];
    snprintf(buf, sizeof(buf), TIMESTAMP_PATTERN, timestamp);
    Uart.Print(buf);
}

void TimingAnalyzer::OutOfSync(const char *stage)
{
    Uart.Print("Probe out of sync: ");
    Uart.Print(stage);
    Uart.Print("\r\n");

    ResetStage();
}

uint32_t TimingAnalyzer::CalculateAirTime(uint8_t payloadLength)
{
    uint32_t symbolDuration = (1U << spreadingFactor) * 1000000 / bandwidth;
    uint32_t preambleDuration = 12 * symbolDuration + symbolDuration / 4;

    uint8_t div = 4 * (spreadingFactor - 2 * lowDataRateOptimization);
    int32_t numPayloadSymbols = (8 * payloadLength - 4 * spreadingFactor + 44 - 20 * implicitHeader + div - 1) / div;
    numPayloadSymbols *= codingRate;
    if (numPayloadSymbols < 0)
        numPayloadSymbols = 0;
    numPayloadSymbols += 8;
    uint32_t payloadDuration = numPayloadSymbols * symbolDuration;

    return preambleDuration + payloadDuration;
}

uint32_t TimingAnalyzer::CalculateTimeoutTime()
{
    uint32_t symbolDuration = (1U << spreadingFactor) * 1000000 / bandwidth;
    return symbolDuration * numTimeoutSymbols;
}
