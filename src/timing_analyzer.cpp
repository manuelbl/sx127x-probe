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

#define TIMESTAMP_PATTERN "%7ld: "

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

    if (result == LoraResultDownlinkInRx1)
        PrintRxAnalysis(1000000, rx1Start - txEndTime, rx1End - txEndTime, payloadLength);
    else
        PrintRxAnalysis(2000000, rx2Start - txEndTime, rx2End - txEndTime, payloadLength);

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
        PrintTimeoutAnalysis(1000000, rx1Start - txEndTime, rx1End - txEndTime);
    }
    else
    {
        rx2End = time;
        result = LoraResultNoDownlink;
        PrintTimeoutAnalysis(2000000, rx2Start - txEndTime, rx2End - txEndTime);
        OnRxTxCompleted();
    }
}

void TimingAnalyzer::PrintRxAnalysis(int32_t expectedStartTime, int32_t windowStartTime, int32_t windowEndTime, int payloadLength)
{
    // HACK: It looks as if the air time calculation fits much better with 2 bytes less...
    int32_t airTime = CalculateAirTime(payloadLength - 2);

    Uart.Printf("         SF%d, %lu Hz, payload = %d bytes, airtime = %ldus\r\n",
            spreadingFactor, bandwidth, payloadLength, airTime);

    int32_t calculatedStart = windowEndTime - airTime;
    Uart.Printf("         Start of preamble (calculated): %ld\r\n", calculatedStart);
}

void TimingAnalyzer::PrintTimeoutAnalysis(int32_t expectedStartTime, int32_t windowStartTime, int32_t windowEndTime)
{
    // The receiver listens for a downlink packet for a given time (timeout window).
    // If a packet preamble is detected during that time, it continues to recieve
    // the packet. If not, it signals a timeout at the end of the window.
    // The optimal timeout window is positioned such that the middle of the window
    // aligns with the middle of the expected preamble. That way the margin for timing
    // errors is the same at the start and the end of the window.
    int32_t timeoutLength = CalculateTime(numTimeoutSymbols);
    int32_t ramupDuration = windowEndTime - windowStartTime - timeoutLength;
    int32_t marginStart = expectedStartTime + CalculateTime(preambleLength - 5) - (windowStartTime + ramupDuration);
    int32_t marginEnd = windowEndTime - (expectedStartTime + CalculateTime(5));

    Uart.Printf("         SF%d, %lu Hz, airtime = %ldus, ramp-up = %ldus\r\n",
            spreadingFactor, bandwidth, timeoutLength, ramupDuration);

    int32_t optimumEndTime = expectedStartTime + (CalculateTime(preambleLength) + timeoutLength) / 2;
    int32_t corr = windowEndTime - optimumEndTime;

    Uart.Printf("         Margin: start = %ldus, end = %ldus\r\n", marginStart, marginEnd);
    Uart.Printf("         Correction for optimum RX window: %ldus\r\n", corr);
}


void TimingAnalyzer::PrintParameters(int32_t duration, int payloadLength)
{
    int32_t airTime = CalculateAirTime(payloadLength);
    int32_t rampupTime = duration - airTime;

    Uart.Printf("         SF%d, %lu Hz, payload = %d bytes, airtime = %ldus, ramp-up = %ldus\r\n",
            spreadingFactor, bandwidth, payloadLength, airTime, rampupTime);
}

void TimingAnalyzer::OnRxTxCompleted()
{
    stage = LoraStageIdle;
    result = LoraResultNoDownlink;
}

void TimingAnalyzer::PrintRelativeTimestamp(int32_t timestamp)
{
    Uart.Printf(TIMESTAMP_PATTERN, timestamp);
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
