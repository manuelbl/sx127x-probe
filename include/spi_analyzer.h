/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * SPI communication analyzer
 */

#ifndef _SPI_ANALYZER_H_
#define _SPI_ANALYZER_H_

#include <stdint.h>
#include <stddef.h>
#include "timing_analyzer.h"

class SpiAnalyzer
{
public:
    SpiAnalyzer(const uint8_t *buf, size_t bufSize, TimingAnalyzer &ta)
        : timingAnalyzer(ta), circularBufferStart(buf), circularBufferEnd(buf + bufSize),
          symbolTimeout(0x64), preambleLength(8) {}
    void OnTrx(uint32_t time, const uint8_t *startTrx, const uint8_t *endTrx);

private:
    void OnRegWrite(uint32_t time, uint8_t reg, uint8_t value);
    void OnOpModeChanged(uint32_t time, uint8_t value);
    void OnSymbTimeoutLsbChanged(uint8_t value);
    void OnModemConfig1(uint8_t value);
    void OnModemConfig2(uint8_t value);
    void OnModemConfig3(uint8_t value);
    void OnPreambleMsbChanged(uint8_t value);
    void OnPreambleLsbChanged(uint8_t value);
    void OnPayloadLengthChanged(uint8_t value);

private:
    TimingAnalyzer &timingAnalyzer;
    const uint8_t *circularBufferStart;
    const uint8_t *circularBufferEnd;
    uint16_t symbolTimeout;
    uint16_t preambleLength;
};

#endif
