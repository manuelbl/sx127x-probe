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

    Uart.Print("---------\r\n");
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

    PrintRelativeTimestamp(time - txEndTime);
    Uart.Print(stage == LoraStageInRx1Window ? "RX1 start\r\n" : "RX2 start\r\n");
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
        stage = LoraStageBeforeRx1Window;
        txEndTime = time;

        PrintRelativeTimestamp(txStartTime - txEndTime);
        Uart.Print("TX start\r\n");
        PrintRelativeTimestamp(0);
        Uart.Print("TX done\r\n");

        PrintParameters(txEndTime - txStartTime, txPayloadLength);
    }
    else if (stage == LoraStageInRx1Window)
    {
        rx1End = time;
        result = LoraResultDownlinkInRx1;
        stage = LoraStageWaitingForData;

        PrintRelativeTimestamp(rx1End - txEndTime);
        Uart.Print("RX1: downlink packet received\r\n");
    }
    else
    {
        rx2End = time;
        result = LoraResultDownlinkInRx2;
        stage = LoraStageWaitingForData;

        PrintRelativeTimestamp(rx2End - txEndTime);
        Uart.Print("RX2: downlink packet received\r\n");
    }
}

void TimingAnalyzer::OnDataReceived(uint8_t payloadLength)
{
    if (stage != LoraStageWaitingForData)
    {
        OutOfSync("reading FIFO");
        return;
    }

    PrintParameters(result == LoraResultDownlinkInRx1 ? rx1End - rx1Start : rx2End - rx2Start, payloadLength);

    OnRxTxCompleted();
}

void TimingAnalyzer::OnTimeoutInterrupt(uint32_t time)
{
    if (stage != LoraStageInRx1Window && stage != LoraStageInRx2Window)
    {
        OutOfSync("timeout interrupt");
        return;
    }

    PrintRelativeTimestamp(time - txEndTime);
    Uart.Print(stage == LoraStageInRx1Window ? "RX1 timeout\r\n" : "RX2 timeout\r\n");

    if (stage == LoraStageInRx1Window)
    {
        stage = LoraStageBeforeRx2Window;
        rx1End = time;
        AnalyzeTimeout(txEndTime + 1000000, time, rx1End - rx1Start);
    }
    else
    {
        rx2End = time;
        result = LoraResultNoDownlink;
        AnalyzeTimeout(txEndTime + 2000000, time, rx2End - rx2Start);
        OnRxTxCompleted();
    }
}

void TimingAnalyzer::AnalyzeTimeout(int32_t expectedStartTime, int32_t windowEndTime, int32_t duration)
{
    // The receiver listens for a downlink packet for a given time (timeout window).
    // If a packet preamble is detected during that time, it continues to recieve
    // the packet. If not, it signals a timeout.
    // The optimal timeout window is positioned such that the middle of the window
    // aligns with the middle of the expected preamble. That way it is least senstive
    // to time diference in both directions.
    int32_t timeoutLength = CalculateTime(numTimeoutSymbols);
    int32_t optimumEndTime = expectedStartTime + CalculateTime(preambleLength) / 2 + timeoutLength;
    int32_t difference = windowEndTime - optimumEndTime;

    snprintf(formatBuf, sizeof(formatBuf),
            "         SF%d, %lu Hz, airtime = %ld, ramp-up = %ld\r\n",
            spreadingFactor, bandwidth, timeoutLength, duration - timeoutLength);
    Uart.Print(formatBuf);

    snprintf(formatBuf, sizeof(formatBuf),
            "         Correction for optimum RX window: %ld us\r\n",
            difference);
    Uart.Print(formatBuf);
}


void TimingAnalyzer::PrintParameters(int32_t duration, int payloadLength)
{
    int32_t airTime = CalculateAirTime(payloadLength);
    int32_t rampupTime = duration - airTime;

    snprintf(formatBuf, sizeof(formatBuf),
            "         SF%d, %lu Hz, payload = %d bytes, airtime = %ld, ramp-up = %ld\r\n",
            spreadingFactor, bandwidth, payloadLength, airTime, rampupTime);
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
