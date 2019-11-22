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

#define TIMESTAMP_PATTERN "%7ld: "

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

    PrintRelativeTimestamp(time - txEndTime);
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

    if (stage == LoraStageTransmitting)
    {
        PrintRelativeTimestamp(0);
        Uart.Print("TX done\r\n");

        stage = LoraStageBeforeRx1Window;
        txEndTime = time;

        int32_t airTime = CalculateAirTime(txPayloadLength);
        uint32_t overallDuration = txEndTime - txStartTime;
        snprintf(formatBuf, sizeof(formatBuf), "         Communicattion overhead: %ld us\r\n",
                overallDuration - airTime);
        Uart.Print(formatBuf);
    }
    else if (stage == LoraStageInRx1Window)
    {
        PrintRelativeTimestamp(time - txEndTime);
        Uart.Print("RX 1 window: downlink done\r\n");

        rx1End = time;
        result = LoraResultDownlinkInRx1;
        stage = LoraStageWaitingForData;
    }
    else
    {
        PrintRelativeTimestamp(time - txEndTime);
        Uart.Print("RX 2 window: downlink done\r\n");

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

    int32_t overallDuration;

    PrintRelativeTimestamp(time - txEndTime);
    Uart.Print("Timeout\r\n");

    if (stage == LoraStageInRx1Window)
    {
        stage = LoraStageBeforeRx2Window;
        rx1End = time;
        overallDuration = time - rx1Start;
        AnalyzeTimeout(txEndTime + 1000000, time);
    }
    else
    {
        rx2End = time;
        result = LoraResultNoDownlink;
        overallDuration = time - rx2Start;
        OnRxTxCompleted();
        AnalyzeTimeout(txEndTime + 2000000, time);
    }

    int32_t timeoutDuration = CalculateTime(numTimeoutSymbols);
    snprintf(formatBuf, sizeof(formatBuf), "         Communicattion overhead: %ld us\r\n",
            overallDuration - timeoutDuration);
    Uart.Print(formatBuf);
}

void TimingAnalyzer::AnalyzeTimeout(int32_t expectedStartTime, int32_t windowEndTime)
{
    // To synchronize the receiver needs 4 preamble symbols in practice.
    // So in optimum receive window the last 4 preamble symbols are in 
    // the middle of the window.
    int32_t timeoutLength = CalculateTime(numTimeoutSymbols);
    int32_t optimumMiddleTime = expectedStartTime + CalculateTime(preambleLength - 2);
    int32_t effectiveMiddle = windowEndTime - timeoutLength / 2;
    int32_t difference = effectiveMiddle - optimumMiddleTime;

    snprintf(formatBuf, sizeof(formatBuf), "         Correction for optimum RX window: %ld us (timeout: %ld us)\r\n",
            difference, timeoutLength);
    Uart.Print(formatBuf);
}

void TimingAnalyzer::OnRxTxCompleted()
{
    stage = LoraStageIdle;
    result = LoraResultNoDownlink;
}

void TimingAnalyzer::PrintRelativeTimestamp(int32_t timestamp)
{
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

int32_t TimingAnalyzer::CalculateAirTime(uint8_t payloadLength)
{
    int32_t symbolDuration = (1U << spreadingFactor) * 1000000 / (int32_t)bandwidth;
    int32_t preambleDuration = (preambleLength + 4) * symbolDuration + symbolDuration / 4;

    int8_t div = 4 * (spreadingFactor - 2 * lowDataRateOptimization);
    int32_t numPayloadSymbols = (8 * payloadLength - 4 * spreadingFactor + 44 - 20 * implicitHeader + div - 1) / div;
    numPayloadSymbols *= codingRate;
    if (numPayloadSymbols < 0)
        numPayloadSymbols = 0;
    numPayloadSymbols += 8;
    int32_t payloadDuration = numPayloadSymbols * symbolDuration;

    return preambleDuration + payloadDuration;
}

int32_t TimingAnalyzer::CalculateTime(int numSymbols)
{
    int32_t symbolDuration = (1U << spreadingFactor) * 1000000 / (int32_t)bandwidth;
    return symbolDuration * numSymbols;
}
