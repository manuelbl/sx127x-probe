/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * LoRa timing analyzer
 */

#ifndef TIMING_ANALYZER_H
#define TIMING_ANALYZER_H

#include <stdint.h>

enum LoraTxRxStage
{
    LoraStageIdle,
    LoraStageTransmitting,
    LoraStageBeforeRx1Window,
    LoraStageInRx1Window,
    LoraStageBeforeRx2Window,
    LoraStageInRx2Window,
    LoraStageWaitingForData
};

enum LoraTxRxResult
{
    LoraResultNoDownlink,
    LoraResultDownlinkInRx1,
    LoraResultDownlinkInRx2
};


class TimingAnalyzer
{
public:
    TimingAnalyzer();

    void OnTxStart(uint32_t time);
    void OnRxStart(uint32_t time);
    void OnDoneInterrupt(uint32_t time);
    void OnTimeoutInterrupt(uint32_t time);
    void OnDataReceived(uint8_t rxPayloadLength);

    void SetRxSymbolTimeout(uint16_t numTimeoutSymbols) { this->numTimeoutSymbols = numTimeoutSymbols; }
    void SetBandwidth(uint32_t bandwidth) { this->bandwidth = bandwidth; }
    void SetCodingRate(uint8_t codingRate) { this->codingRate = codingRate; }
    void SetImplicitHeader(uint8_t implicitHeader) { this->implicitHeader = implicitHeader; }
    void SetSpreadingFactor(uint8_t spreadingFactor) { this->spreadingFactor = spreadingFactor; }
    void SetCrcOn(uint8_t crcOn) { this->crcOn = crcOn; }
    void SetPreambleLength(uint16_t preambleLength) { this->preambleLength = preambleLength; }
    void SetTxPayloadLength(uint8_t txPayloadLength) { this->txPayloadLength = txPayloadLength; }
    void SetLowDataRateOptimization(uint8_t lowDataRateOptimization) { this->lowDataRateOptimization = lowDataRateOptimization; }

private:
    void ResetStage();
    void OnRxTxCompleted();

    void PrintRxAnalysis(int32_t expectedStartTime, int32_t windowStartTime, int32_t windowEndTime, int payloadLength);
    void PrintTimeoutAnalysis(int32_t expectedStartTime, int32_t windowStartTime, int32_t windowEndTime);
    void PrintParameters(int32_t duration, int payloadLength);
    static void PrintRelativeTimestamp(int32_t timestamp);

    void OutOfSync(const char* stage);
    int32_t CalculateAirTime(uint8_t payloadLength);
    int32_t CalculateTime(int numSymbols);

    LoraTxRxStage stage;
    LoraTxRxResult result;
    uint32_t txStartTime;
    uint32_t txEndTime;
    uint32_t rx1Start;
    uint32_t rx1End;
    uint32_t rx2Start;
    uint32_t rx2End;

    uint32_t bandwidth;
    uint16_t numTimeoutSymbols;
    uint8_t codingRate;
    uint8_t implicitHeader;
    uint8_t spreadingFactor;
    uint8_t crcOn;
    uint16_t preambleLength;
    uint8_t txPayloadLength;
    uint8_t lowDataRateOptimization;
};

#endif
