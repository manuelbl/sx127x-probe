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
#include <cmath>

#define TIMESTAMP_PATTERN "%8ld: "

// Minimum number of preamble symbols required to detect packet
#define MIN_RX_SYMBOLS 6


TimingAnalyzer::TimingAnalyzer()
    : sampleNo(0), stage(LoraStageIdle), result(LoraResultNoDownlink),
      txUncalibratedStartTime(0), txStartTime(0), txUncalibratedEndTime(0),
      rx1Start(0), rx1End(0), rx2Start(0), rx2End(0),
      longRangeMode(LongrangeModeLora), bandwidth(125000), numTimeoutSymbols(0x64), codingRate(5),
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

    sampleNo++;
    Serial.Printf("--------  Sample %d  --------\r\n", sampleNo);
    stage = LoraStageTransmitting;
    txUncalibratedStartTime = time;
}

void TimingAnalyzer::OnRxStart(uint32_t time)
{
    if (stage != LoraStageBeforeRx1Window && stage != LoraStageBeforeRx2Window)
    {
        OutOfSync("RX start");
        return;
    }

    int32_t t = CalibratedTime(time - txUncalibratedEndTime);

    if (stage == LoraStageBeforeRx1Window)
    {
        stage = LoraStageInRx1Window;
        rx1Start = t;
    }
    else
    {
        stage = LoraStageInRx2Window;
        rx2Start = t;
    }

    PrintRelativeTimestamp(t);
    Serial.Printf("RX%c start\r\n", stage == LoraStageInRx1Window ? '1' : '2');
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
        txUncalibratedEndTime = time;
        txStartTime = CalibratedTime(txUncalibratedStartTime - txUncalibratedEndTime);
        stage = LoraStageBeforeRx1Window;

        PrintRelativeTimestamp(txStartTime);
        Serial.Print("TX start\r\n");
        PrintRelativeTimestamp(0);
        Serial.Print("TX done\r\n");

        PrintParameters(-txStartTime, txPayloadLength);
    }
    else if (stage == LoraStageInRx1Window)
    {
        rx1End = CalibratedTime(time - txUncalibratedEndTime);
        result = LoraResultDownlinkInRx1;
        stage = LoraStageWaitingForData;

        PrintRelativeTimestamp(rx1End);
        Serial.Print("RX1: downlink packet received\r\n");
    }
    else
    {
        rx2End = CalibratedTime(time - txUncalibratedEndTime);
        result = LoraResultDownlinkInRx2;
        stage = LoraStageWaitingForData;

        PrintRelativeTimestamp(rx2End);
        Serial.Print("RX2: downlink packet received\r\n");
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
        PrintRxAnalysis(rx1Start, rx1End, payloadLength);
    else
        PrintRxAnalysis(rx2Start, rx2End, payloadLength);

    OnRxTxCompleted();
}

void TimingAnalyzer::OnTimeoutInterrupt(uint32_t time)
{
    if (stage != LoraStageInRx1Window && stage != LoraStageInRx2Window)
    {
        OutOfSync("timeout interrupt");
        return;
    }

    int32_t t = CalibratedTime(time - txUncalibratedEndTime);

    PrintRelativeTimestamp(t);
    Serial.Print(stage == LoraStageInRx1Window ? "RX1 timeout\r\n" : "RX2 timeout\r\n");

    if (stage == LoraStageInRx1Window)
    {
        stage = LoraStageBeforeRx2Window;
        rx1End = t;
        PrintTimeoutAnalysis(rx1Start, rx1End, rx1Correction);
    }
    else
    {
        rx2End = t;
        result = LoraResultNoDownlink;
        PrintTimeoutAnalysis(rx2Start, rx2End, rx2Correction);
        OnRxTxCompleted();
    }
}

void TimingAnalyzer::PrintRxAnalysis(int32_t windowStartTime, int32_t windowEndTime, int payloadLength)
{
    // HACK: It looks as if the air time calculation fits much better with 2 bytes less...
    int32_t airTime = PayloadAirTime(payloadLength - 2);

    Serial.Printf("          SF%d, %lu Hz, payload = %d bytes, airtime = %ldus\r\n",
            spreadingFactor, bandwidth, payloadLength, airTime);

    int32_t calculatedStartTime = windowEndTime - airTime;
    Serial.Printf("          Start of preamble (calculated): %ld\r\n", calculatedStartTime);

    // Ramp-up time is not known but assumed to be 300us.
    int32_t marginStart = calculatedStartTime + SymbolDuration(preambleLength - MIN_RX_SYMBOLS) - windowStartTime - 300;
    Serial.Printf("          Margin: start = %ldus\r\n", marginStart);
}

void TimingAnalyzer::PrintTimeoutAnalysis(int32_t windowStartTime, int32_t windowEndTime, RollingAvergage& buffer)
{
    // Round to nearest second
    int32_t expectedStartTime = (windowStartTime + 500000) / 1000000 * 1000000;

    // The receiver listens for a downlink packet for a given time (timeout window).
    // If a packet preamble is detected during that time, it continues to recieve
    // the packet. If not, it signals a timeout at the end of the window.
    // The optimal timeout window is positioned such that the middle of the window
    // aligns with the middle of the expected preamble. That way the margin for timing
    // errors is the same at the start and the end of the window.
    int32_t timeoutLength = SymbolDuration(numTimeoutSymbols);
    int32_t ramupDuration = windowEndTime - windowStartTime - timeoutLength;
    int32_t marginStart = expectedStartTime + SymbolDuration(preambleLength - MIN_RX_SYMBOLS) - windowStartTime - ramupDuration;
    int32_t marginEnd = windowEndTime - (expectedStartTime + SymbolDuration(MIN_RX_SYMBOLS));

    Serial.Printf("          SF%d, %lu Hz, airtime = %ldus, ramp-up = %ldus\r\n",
            spreadingFactor, bandwidth, timeoutLength, ramupDuration);

    int32_t optimumEndTime = expectedStartTime + (SymbolDuration(preambleLength) + timeoutLength) / 2;
    int32_t corr = windowEndTime - optimumEndTime;
    buffer.AddValue(corr);

    Serial.Printf("          Margin: start = %ldus, end = %ldus\r\n", marginStart, marginEnd);
    Serial.Printf("          Correction for optimum RX window: %ldus\r\n", corr);
    Serial.Printf("          Correction mean: %ldus variance %ld\r\n", buffer.Mean(), buffer.Variance());
}


void TimingAnalyzer::PrintParameters(int32_t duration, int payloadLength)
{
    int32_t airTime = PayloadAirTime(payloadLength);
    int32_t rampupTime = duration - airTime;

    if (longRangeMode == LongrangeModeLora) {
        Serial.Printf("          SF%d, %lu Hz, payload = %d bytes, airtime = %ldus, ramp-up = %ldus\r\n",
                spreadingFactor, bandwidth, payloadLength, airTime, rampupTime);
    } else {
        Serial.Printf("          FSK, %lu Hz, payload = %d bytes, airtime = %ldus, ramp-up = %ldus\r\n",
                bandwidth, payloadLength, airTime, rampupTime);
    }
}

void TimingAnalyzer::OnRxTxCompleted()
{
    stage = LoraStageIdle;
    result = LoraResultNoDownlink;
}

void TimingAnalyzer::PrintRelativeTimestamp(int32_t timestamp)
{
    Serial.Printf(TIMESTAMP_PATTERN, timestamp);
}

void TimingAnalyzer::OutOfSync(const char *stage)
{
    Serial.Print("Probe out of sync: ");
    Serial.Print(stage);
    Serial.Print("\r\n");

    ResetStage();
}

int32_t TimingAnalyzer::PayloadAirTime(uint8_t payloadLength)
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

int32_t TimingAnalyzer::SymbolDuration(int numSymbols)
{
    int32_t symbolDuration = (1U << spreadingFactor) * 1000000 / (int32_t)bandwidth;
    return symbolDuration * numSymbols;
}
